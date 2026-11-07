#include "Textures.h"
#include "CorridorStyle.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {
// Derive surface grain from pixel coordinates so texture generation does not consume the maze
// generator's random sequence.
void grain(Image& image, int strength) {
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            unsigned int hash = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
            hash = (hash ^ (hash >> 13)) * 1274126177u;
            int amount = (int)((hash ^ (hash >> 16)) % (2 * strength + 1)) - strength;
            Color color = GetImageColor(image, x, y);
            auto adjust = [amount](unsigned char channel) {
                int value = channel + amount;
                return (unsigned char)(value < 0 ? 0 : value > 255 ? 255 : value);
            };
            ImageDrawPixel(&image, x, y, {adjust(color.r), adjust(color.g), adjust(color.b), 255});
        }
    }
}

// Draw light top/left edges and dark bottom/right edges to give a rectangular texture panel a
// beveled appearance.
void frame(Image& image, int x, int y, int width, int height, Color light, Color dark) {
    ImageDrawRectangle(&image, x, y, width, 2, light);
    ImageDrawRectangle(&image, x, y, 2, height, light);
    ImageDrawRectangle(&image, x, y + height - 2, width, 2, dark);
    ImageDrawRectangle(&image, x + width - 2, y, 2, height, dark);
}

// Upload the image and free its CPU pixels, then enable mipmaps, anisotropic filtering and
// repeating texture coordinates.
Texture2D upload(Image& image) {
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    return texture;
}

// Draw a small cubic scroll in image space. The ceiling ornaments use these curves without
// loading artwork or consuming random state.
void scroll(Image& image, Vector2 a, Vector2 b, Vector2 c, Vector2 d, int width, Color color) {
    Vector2 previous = a;
    for (int step = 1; step <= 18; ++step) {
        const float t = step / 18.0f, u = 1 - t;
        Vector2 next{u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x + t * t * t * d.x,
                     u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y + t * t * t * d.y};
        ImageDrawLineEx(&image, previous, next, width, color);
        previous = next;
    }
}

void floralMotif(Image& image, float x, float y, float size, Color thread, Color shadow) {
    for (int layer = 0; layer < 2; ++layer) {
        const Color color = layer ? thread : shadow;
        const int width = std::max(1, (int)((layer ? 2.1f : 4.8f) * size));
        for (int quarter = 0; quarter < 4; ++quarter) {
            auto point = [&](float px, float py) {
                for (int turn = 0; turn < quarter; ++turn) {
                    const float old = px;
                    px = -py;
                    py = old;
                }
                return Vector2{x + px * size, y + py * size};
            };
            auto curve = [&](Vector2 a, Vector2 b, Vector2 c, Vector2 d) {
                scroll(image, point(a.x, a.y), point(b.x, b.y), point(c.x, c.y), point(d.x, d.y),
                       width, color);
            };
            curve({0, -4}, {-17, -18}, {-11, -34}, {0, -43});
            curve({0, -43}, {11, -34}, {17, -18}, {0, -4});
            curve({0, -8}, {0, -22}, {1, -29}, {0, -37});
            curve({0, -23}, {30, -57}, {49, -22}, {25, -10});
            curve({25, -10}, {8, -3}, {6, -31}, {22, -27});
            curve({22, -27}, {33, -23}, {29, -15}, {24, -20});
            ImageDrawLineEx(&image, point(0, -57), point(57, 0), std::max(1, width - 1), color);
            ImageDrawLineEx(&image, point(16, -42), point(24, -43), width, color);
            ImageDrawLineEx(&image, point(40, -18), point(43, -25), width, color);
        }
    }
    ImageDrawCircle(&image, (int)x, (int)y, std::max(1, (int)(5 * size)), shadow);
    ImageDrawCircle(&image, (int)x, (int)y, std::max(1, (int)(2 * size)), thread);
}

Image materialArtwork(const char* filename) {
    std::string path = std::string(GetApplicationDirectory()) + "assets/" + filename;
    if (!FileExists(path.c_str()))
        path = std::string("assets/") + filename;
    return FileExists(path.c_str()) ? LoadImage(path.c_str()) : Image{};
}

// The diffuse wall bay follows the loading-screen artwork: dark damask, gilt scroll corners,
// marble wainscoting and layered moldings. Real sconces provide the room's light separately.
Texture2D wallMaterial(const MazePalette& palette) {
    Image artwork = materialArtwork("hotel-wall-panels.png");
    if (artwork.data) {
        ImageResize(&artwork, 1024, 1024);
        return upload(artwork);
    }
    const CorridorStyle style = corridorStyle(palette);
    Image image = GenImageColor(1024, 1024, style.panel);
    ImageDrawRectangle(&image, 0, 700, 1024, 324, style.trim);
    for (int x = 0; x < 1024; ++x) {
        const float fiber = .018f + .006f * std::sin(x * .39f) + .008f * std::sin(x * .071f);
        ImageDrawRectangle(&image, x, 0, 1, 700, ColorLerp(style.panel, palette.paper, fiber));
        const float wood = .03f + .025f * std::sin(x * .19f + std::sin(x * .037f));
        ImageDrawRectangle(&image, x, 700, 1, 324, ColorLerp(style.trim, style.trimLight, wood));
    }
    for (int left : {44, 532}) {
        ImageDrawRectangle(&image, left, 756, 448, 190,
                           ColorLerp(style.trim, style.trimShade, .24f));
        frame(image, left, 756, 448, 190, style.trimShade, style.trimLight);
        frame(image, left + 7, 763, 434, 176, style.trimLight, style.trimShade);
        ImageDrawRectangle(&image, left + 13, 769, 422, 164, style.trim);
    }
    frame(image, 62, 78, 900, 590, style.trimLight, style.trimShade);
    frame(image, 68, 84, 888, 578, style.brass, style.trimLight);
    for (int left : {0, 990}) {
        ImageDrawRectangle(&image, left, 0, 34, 1024, style.trim);
        ImageDrawRectangle(&image, left + 5, 0, 2, 1024, style.trimLight);
    }
    for (int y : {12, 28, 45, 675, 683, 699, 961, 984, 1009}) {
        ImageDrawRectangle(&image, 0, y, 1024, 4, style.trimLight);
        ImageDrawRectangle(&image, 0, y + 4, 1024, 3, style.trimShade);
    }
    grain(image, 1);
    return upload(image);
}

// A separate material lets the pilaster fronts and returns use stone rather than stretched wall
// wallpaper. It shares the same ownership and filtering as the other room materials.
Texture2D marbleMaterial(const MazePalette& palette) {
    Image image = materialArtwork("hotel-marble.png");
    if (!image.data) {
        image = GenImageColor(512, 512, corridorStyle(palette).plaster);
        grain(image, 3);
    }
    ImageResize(&image, 512, 512);
    return upload(image);
}

void diamond(Image& image, int x, int y, int radiusX, int radiusY, Color color) {
    const Vector2 top{(float)x, (float)(y - radiusY)}, bottom{(float)x, (float)(y + radiusY)};
    const Vector2 left{(float)(x - radiusX), (float)y}, right{(float)(x + radiusX), (float)y};
    ImageDrawTriangle(&image, top, left, right, color);
    ImageDrawTriangle(&image, left, bottom, right, color);
}

void star(Image& image, int x, int y, int radiusX, int radiusY, Color color) {
    // Eight broad points stay recognizable in mipmaps; tiny stitches are secondary to the shape.
    Vector2 vertices[18]{};
    vertices[0] = {(float)x, (float)y};
    for (int point = 0; point <= 16; ++point) {
        const float angle = point * PI / 8.0f;
        const float radius = point % 2 ? .47f : 1.0f;
        vertices[point + 1] = {x + std::cos(angle) * radiusX * radius,
                               y + std::sin(angle) * radiusY * radius};
    }
    ImageDrawTriangleFan(&image, vertices, 18, color);
}

// Angular ram-horn ornaments fill the spaces between medallions in the reference weave.
void wovenHook(Image& image, int x, int y, int radius, Color color, Color center) {
    const int width = std::max(3, radius / 6);
    for (int quarter = 0; quarter < 4; ++quarter) {
        auto point = [&](int px, int py) {
            for (int turn = 0; turn < quarter; ++turn) {
                const int old = px;
                px = -py;
                py = old;
            }
            return Vector2{(float)(x + px), (float)(y + py)};
        };
        ImageDrawLineEx(&image, point(0, 0), point(0, -radius), width, color);
        ImageDrawLineEx(&image, point(0, -radius), point(radius, -radius), width, color);
        ImageDrawLineEx(&image, point(radius, -radius), point(radius, -radius / 3), width, color);
        ImageDrawLineEx(&image, point(radius, -radius / 3), point(radius / 2, -radius / 3), width,
                        color);
    }
    diamond(image, x, y, radius / 3, radius / 3, center);
}

// Layered Kazak-style medallions use filled shapes and wide stitches instead of hairline art.
void medallion(Image& image, int x, int y, int radius, Color field, Color dye, Color yarn,
               Color dark) {
    diamond(image, x, y, radius, radius, dark);
    diamond(image, x, y, radius - 9, radius - 9, yarn);
    diamond(image, x, y, radius - 20, radius - 20, dye);
    diamond(image, x, y, radius - 39, radius - 39, dark);
    diamond(image, x, y, radius - 48, radius - 48, yarn);
    diamond(image, x, y, radius - 61, radius - 61, dye);
    diamond(image, x, y, (int)(radius * .60f), (int)(radius * .60f), yarn);
    diamond(image, x, y, (int)(radius * .55f), (int)(radius * .55f), field);
    diamond(image, x, y, (int)(radius * .43f), (int)(radius * .43f), dye);
    star(image, x, y, (int)(radius * .29f), (int)(radius * .29f), yarn);
    star(image, x, y, (int)(radius * .21f), (int)(radius * .21f), dark);
    diamond(image, x, y, (int)(radius * .09f), (int)(radius * .09f), yarn);
    for (int horizontal : {-1, 1})
        for (int vertical : {-1, 1}) {
            wovenHook(image, x + horizontal * radius / 3, y + vertical * radius / 3, 14, yarn,
                      dark);
            diamond(image, x + horizontal * radius / 6, y + vertical * radius / 6, 12, 12, field);
        }
    for (int quarter = 0; quarter < 4; ++quarter) {
        auto point = [&](int px, int py) {
            for (int turn = 0; turn < quarter; ++turn) {
                const int old = px;
                px = -py;
                py = old;
            }
            return Vector2{(float)(x + px), (float)(y + py)};
        };
        // Stepped hooks and opposing stitches echo the woven border around the reference diamonds.
        for (int step = 0; step < 7; ++step) {
            const int a = 38 + step * (radius - 90) / 7, b = radius - 26 - a;
            const Vector2 stitch = point(a, -b);
            diamond(image, (int)stitch.x, (int)stitch.y, 7, 7, yarn);
            const Vector2 tip = point(a - 8, -b - 22);
            ImageDrawLineEx(&image, point(a - 8, -b - 4), tip, 5, yarn);
            ImageDrawLineEx(&image, tip, point(a + 5, -b - 22), 5, yarn);
        }
        for (int step = 0; step < 5; ++step) {
            const int a = 25 + step * (radius / 2 - 40) / 5;
            const Vector2 stitch = point(a, -(int)(radius * .58f) + a);
            diamond(image, (int)stitch.x, (int)stitch.y, 5, 5, dark);
        }
    }
}

// Fit the full-color textile artwork into its own atlas region without applying the maze palette.
void carpetArtwork(Image& atlas, const char* filename, int left, int width) {
    Image source = materialArtwork(filename);
    if (!source.data)
        return;
    ImageResize(&source, width, CarpetLayout::FieldSize);
    ImageDraw(&atlas, source, {0, 0, (float)width, (float)CarpetLayout::FieldSize},
              {(float)left, 0, (float)width, (float)CarpetLayout::FieldSize}, WHITE);
    UnloadImage(source);
}

// Cache the same cream, crimson and near-black carpet for every run. Center and border remain
// separate atlas regions so borders can follow walls and miter around corridor corners.
Texture2D floorMaterial() {
    using namespace CarpetLayout;
    static const std::vector<Color> pixels = [] {
        constexpr Color field{214, 199, 164, 255}, dye{165, 38, 29, 255};
        constexpr Color yarn{225, 207, 165, 255}, dark{33, 24, 21, 255};
        // Procedural motifs keep the material usable if the packaged artwork is unavailable.
        Image image = GenImageColor(AtlasWidth, FieldSize, field);
        medallion(image, 512, 512, 332, field, dye, yarn, dark);
        for (int x : {238, 786})
            for (int y : {176, 848}) {
                diamond(image, x, y, 65, 65, dye);
                diamond(image, x, y, 54, 54, yarn);
                diamond(image, x, y, 44, 44, dark);
                star(image, x, y, 34, 34, yarn);
                diamond(image, x, y, 13, 13, dye);
            }
        for (int y : {0, 1024}) {
            star(image, 512, y, 73, 73, yarn);
            star(image, 512, y, 52, 52, dye);
        }
        for (int x : {190, 834})
            for (int y = 320; y <= 704; y += 96) {
                star(image, x, y, 31, 31, dye);
                star(image, x, y, 20, 20, yarn);
            }
        for (int x : {350, 674})
            for (int y : {88, 936}) {
                wovenHook(image, x, y, 32, yarn, dye);
                diamond(image, x, y, 10, 10, dye);
            }
        for (int y : {256, 768})
            for (int x : {190, 834})
                wovenHook(image, x, y, 23, ColorLerp(yarn, dye, .28f), dye);
        for (int y : {115, 909}) {
            diamond(image, 512, y, 37, 37, dye);
            diamond(image, 512, y, 27, 27, yarn);
            star(image, 512, y, 18, 18, field);
        }
        // A broad repeating star band sits between narrow guard stripes and a stepped meander.
        // The dark guard gap prevents border colors bleeding into the field's mipmapped repeat
        // edge.
        const int border = BorderOffset;
        ImageDrawRectangle(&image, border, 0, BorderSize, FieldSize, dye);
        for (int offset : {0, 19, 40, 178, 192, 232, 247})
            ImageDrawRectangle(&image, border + offset, 0, offset == 192 ? 40 : 6, FieldSize,
                               offset == 192 ? yarn : dark);
        for (int y = 64; y < FieldSize; y += 128) {
            star(image, border + 109, y, 65, 50, yarn);
            star(image, border + 109, y, 46, 35, dark);
            diamond(image, border + 109, y, 24, 19, dye);
            for (int end : {-1, 1})
                diamond(image, border + 109 + end * 52, y + 56, 16, 12, yarn);
        }
        for (int y = -32; y < FieldSize; y += 64) {
            ImageDrawRectangle(&image, border + 202, y, 20, 8, dye);
            ImageDrawRectangle(&image, border + 202, y, 7, 36, dye);
            ImageDrawRectangle(&image, border + 202, y + 29, 20, 7, dye);
            ImageDrawRectangle(&image, border + 215, y + 29, 7, 35, dye);
        }
        carpetArtwork(image, "carpet-persian-field.png", 0, FieldSize);
        carpetArtwork(image, "carpet-persian-border.png", BorderOffset, BorderSize);
        // Low-contrast weave preserves large filled motifs and lets mipmaps remove subpixel
        // threads.
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < BorderOffset + BorderSize; ++x) {
                const Color color = GetImageColor(image, x, y);
                ImageDrawPixel(&image, x, y, ColorLerp(color, (x + y) % 2 ? dark : field, .035f));
            }
        grain(image, 2);
        Color* colors = LoadImageColors(image);
        std::vector<Color> result(colors, colors + AtlasWidth * FieldSize);
        UnloadImageColors(colors);
        UnloadImage(image);
        return result;
    }();
    Image image = GenImageColor(AtlasWidth, FieldSize, BLANK);
    std::copy(pixels.begin(), pixels.end(), static_cast<Color*>(image.data));
    return upload(image);
}

// Recessed plaster coffers and delicate corner flourishes replace the acoustic ceiling tiles.
Texture2D ceilingMaterial(const MazePalette& palette) {
    const CorridorStyle style = corridorStyle(palette);
    Image image = GenImageColor(512, 512, style.ceiling);
    ImageDrawRectangle(&image, 43, 43, 426, 426, style.ceilingInset);
    for (int inset : {10, 22, 37, 47})
        frame(image, inset, inset, 512 - inset * 2, 512 - inset * 2,
              ColorLerp(style.ceiling, palette.paper, .20f),
              ColorLerp(style.ceiling, style.trimShade, .24f));
    for (int x : {88, 424})
        for (int y : {88, 424})
            floralMotif(image, (float)x, (float)y, .43f,
                        ColorLerp(style.ceilingInset, style.trim, .22f), style.ceilingInset);
    grain(image, 1);
    return upload(image);
}

// Center a sign label inside its atlas slot while keeping its glyphs inside the available width.
void signText(Image& image, const char* text, int center, int y, int width, int fontSize,
              Color color) {
    while (fontSize > 10 && MeasureText(text, fontSize) > width)
        --fontSize;
    ImageDrawText(&image, text, center - MeasureText(text, fontSize) / 2, y, fontSize, color);
}

// Draw one padded sign frame inside the atlas at the supplied vertical slot offset.
void signBacking(Image& image, int top, Color accent, const MazePalette& palette) {
    ImageDrawRectangle(&image, 8, top + 8, 496, 112, Fade(palette.trim, .96f));
    frame(image, 8, top + 8, 496, 112, ColorLerp(palette.trim, palette.paper, .28f), palette.trim);
    ImageDrawRectangle(&image, 17, top + 17, 3, 94, accent);
    ImageDrawRectangle(&image, 30, top + 106, 452, 1, ColorLerp(palette.trim, palette.accent, .2f));
    for (int x : {16, 495})
        for (int y : {16, 111}) {
            ImageDrawCircle(&image, x, top + y, 2, palette.accent);
        }
}

// Build the four-slot decorative sign atlas, upload it with clamped UV wrapping, and release the
// source image.
Texture2D decalMaterial(const MazePalette& palette) {
    const Color AMBER = ColorLerp(palette.accent, palette.paper, .38f);
    const Color PAPER = palette.paper;
    const Color MUTED = ColorLerp(palette.accent, palette.paper, .6f);
    constexpr Color MINT = {113, 216, 182, 255};
    Image image = GenImageColor(512, 512, BLANK);

    signBacking(image, 0, AMBER, palette);
    // Construct the first sign's labyrinth emblem from line segments without encoding a direction
    // through the maze.
    ImageDrawRectangle(&image, 40, 37, 52, 3, AMBER);
    ImageDrawRectangle(&image, 40, 37, 3, 52, AMBER);
    ImageDrawRectangle(&image, 89, 37, 3, 52, AMBER);
    ImageDrawRectangle(&image, 56, 86, 36, 3, AMBER);
    ImageDrawRectangle(&image, 56, 54, 3, 35, AMBER);
    ImageDrawRectangle(&image, 56, 54, 20, 3, AMBER);
    signText(image, "LABYRINTH", 297, 31, 370, 36, PAPER);
    signText(image, "EXPLORER FACILITY", 297, 78, 370, 16, MUTED);

    signBacking(image, 128, MINT, palette);
    signText(image, "07", 69, 169, 70, 40, MINT);
    ImageDrawRectangle(&image, 116, 155, 1, 73, {54, 83, 87, 255});
    signText(image, "SECTOR", 303, 159, 334, 34, PAPER);
    signText(image, "SURVEY ZONE", 303, 206, 334, 16, MUTED);

    signBacking(image, 256, AMBER, palette);
    for (int i = 0; i < 4; ++i) {
        // Add repeated diagonal maintenance markings to the sign; their orientation has no
        // navigation meaning.
        int y = 286 + i * 16;
        ImageDrawLine(&image, 34, y + 9, 49, y - 6, AMBER);
        ImageDrawLine(&image, 35, y + 9, 50, y - 6, AMBER);
        ImageDrawLine(&image, 463, y + 9, 478, y - 6, AMBER);
        ImageDrawLine(&image, 464, y + 9, 479, y - 6, AMBER);
    }
    signText(image, "MAINTENANCE", 256, 287, 390, 32, PAPER);
    signText(image, "KEEP ACCESS CLEAR", 256, 334, 390, 16, AMBER);

    signBacking(image, 384, MINT, palette);
    signText(image, "FIND YOUR OWN WAY", 262, 414, 444, 30, PAPER);
    signText(image, "EVERY TURN TELLS A STORY", 262, 462, 444, 16, MUTED);
    Texture2D texture = upload(image);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    return texture;
}
}

// Create the room materials and return their GPU handles as one resource group.
Textures createTextures(const MazePalette& palette) {
    return {wallMaterial(palette), floorMaterial(), ceilingMaterial(palette),
            decalMaterial(palette), marbleMaterial(palette)};
}

// Adopt handles returned by material creation without duplicating their ownership.
Textures::Textures(Texture2D wall, Texture2D floor, Texture2D ceiling, Texture2D decals,
                   Texture2D marble) noexcept
    : wallTexture(wall), floorTexture(floor), ceilingTexture(ceiling), decalTexture(decals),
      marbleTexture(marble) {
}

// Release resources automatically at the end of their owner's lifetime.
Textures::~Textures() {
    reset();
}

// Empty the source while transferring handles, so only the destination unloads them.
Textures::Textures(Textures&& other) noexcept
    : wallTexture(std::exchange(other.wallTexture, {})),
      floorTexture(std::exchange(other.floorTexture, {})),
      ceilingTexture(std::exchange(other.ceilingTexture, {})),
      decalTexture(std::exchange(other.decalTexture, {})),
      marbleTexture(std::exchange(other.marbleTexture, {})) {
}

// Release the destination's old resources before adopting a replacement, including partial material
// sets.
Textures& Textures::operator=(Textures&& other) noexcept {
    if (this != &other) {
        reset();
        wallTexture = std::exchange(other.wallTexture, {});
        floorTexture = std::exchange(other.floorTexture, {});
        ceilingTexture = std::exchange(other.ceilingTexture, {});
        decalTexture = std::exchange(other.decalTexture, {});
        marbleTexture = std::exchange(other.marbleTexture, {});
    }
    return *this;
}

// Require every material before publishing a new palette to the renderer.
bool Textures::valid() const {
    return wallTexture.id && floorTexture.id && ceilingTexture.id && decalTexture.id &&
           marbleTexture.id;
}

// Clear handles after unloading so destruction and move assignment can share the same cleanup path.
void Textures::reset() noexcept {
    for (Texture2D* texture :
         {&wallTexture, &floorTexture, &ceilingTexture, &decalTexture, &marbleTexture}) {
        if (texture->id)
            UnloadTexture(*texture);
        *texture = {};
    }
}
