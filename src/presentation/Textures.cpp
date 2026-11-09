#include "Textures.h"
#include "CorridorStyle.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {
// Add deterministic pixel-coordinate grain to an image without consuming gameplay random state.
// strength controls the variation, allowing repeatable surface detail while keeping seeded mazes
// unchanged.
void grain(Image& image, int strength) {
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            unsigned int hash = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
            hash = (hash ^ (hash >> 13)) * 1274126177u;
            int amount = (int)((hash ^ (hash >> 16)) % (2 * strength + 1)) - strength;
            Color color = GetImageColor(image, x, y);
            // Offset one color byte by the deterministic grain amount and clamp it to the valid
            // channel range. Applying the same adjustment to channels changes brightness without
            // unrelated hue shifts.
            auto adjust = [amount](unsigned char channel) {
                int value = channel + amount;
                return (unsigned char)(value < 0 ? 0 : value > 255 ? 255 : value);
            };
            ImageDrawPixel(&image, x, y, {adjust(color.r), adjust(color.g), adjust(color.b), 255});
        }
    }
}

// Draw light top and left edges plus dark bottom and right edges around an image-space rectangle.
// The paired colors create a shallow beveled-panel appearance in the generated texture.
void frame(Image& image, int x, int y, int width, int height, Color light, Color dark) {
    ImageDrawRectangle(&image, x, y, width, 2, light);
    ImageDrawRectangle(&image, x, y, 2, height, light);
    ImageDrawRectangle(&image, x, y + height - 2, width, 2, dark);
    ImageDrawRectangle(&image, x + width - 2, y, 2, height, dark);
}

// Upload a CPU Image to a GPU texture and release the image's pixels afterward. Configure
// mipmaps, anisotropic filtering, and repeating coordinates; the returned texture must be owned
// and unloaded exactly once.
Texture2D upload(Image& image) {
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    return texture;
}

// Approximate a cubic decorative curve with image-space strokes using the supplied control points
// and width. Ornament generation uses this without external curve assets or random draws.
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

// Draw a fourfold damask ornament around an image-space center, scaling its curves and stroke
// widths by size. A shadow pass beneath the thread-colored pass gives the pattern depth before
// GPU upload.
void floralMotif(Image& image, float x, float y, float size, Color thread, Color shadow) {
    for (int layer = 0; layer < 2; ++layer) {
        const Color color = layer ? thread : shadow;
        const int width = std::max(1, (int)((layer ? 2.1f : 4.8f) * size));
        for (int quarter = 0; quarter < 4; ++quarter) {
            // Rotate and scale a motif-local point for the current quarter-turn before
            // translating it to the image center. Four rotations reuse the same decorative curve
            // definitions.
            auto point = [&](float px, float py) {
                for (int turn = 0; turn < quarter; ++turn) {
                    const float old = px;
                    px = -py;
                    py = old;
                }
                return Vector2{x + px * size, y + py * size};
            };
            // Transform four motif-local control points and draw their cubic scroll with the
            // current layer's width and color. The enclosing shadow and thread passes reuse this
            // curve callback.
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

// Look for a material image in packaged assets beside the executable, then in src/assets for a
// development run. Return an empty Image when unavailable so material generation can use its
// procedural fallback.
Image materialArtwork(const char* filename) {
    std::string path = std::string(GetApplicationDirectory()) + "assets/" + filename;
    if (!FileExists(path.c_str()))
        path = std::string("src/assets/") + filename;
    return FileExists(path.c_str()) ? LoadImage(path.c_str()) : Image{};
}

// Build the wall's CPU artwork from hotel damask, stone, and layered molding details, using
// available image assets and procedural ornament. Upload the completed surface separately from
// real-time lamp lighting.
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

// Create a separate stone texture for pilaster fronts and returns so architectural pieces do not
// stretch the wall wallpaper. Return an owned GPU texture using the same filtering and cleanup
// conventions as other materials.
Texture2D marbleMaterial(const MazePalette& palette) {
    Image image = materialArtwork("hotel-marble.png");
    if (!image.data) {
        image = GenImageColor(512, 512, corridorStyle(palette).plaster);
        grain(image, 3);
    }
    ImageResize(&image, 512, 512);
    return upload(image);
}

// Fill an image-space diamond with two triangles using independent horizontal and vertical radii.
// Carpet ornament helpers reuse this primitive for broad shapes that survive texture filtering.
void diamond(Image& image, int x, int y, int radiusX, int radiusY, Color color) {
    const Vector2 top{(float)x, (float)(y - radiusY)}, bottom{(float)x, (float)(y + radiusY)};
    const Vector2 left{(float)(x - radiusX), (float)y}, right{(float)(x + radiusX), (float)y};
    ImageDrawTriangle(&image, top, left, right, color);
    ImageDrawTriangle(&image, left, bottom, right, color);
}

// Fill an eight-point carpet motif with a triangle fan around its image-space center. Alternate
// outer and inner radii so the broad silhouette stays recognizable in lower mip levels.
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

// Draw an angular ram-horn motif into the carpet image using the supplied center, radius, and
// colors. The broad stepped ornament fills spaces between the larger textile medallions.
void wovenHook(Image& image, int x, int y, int radius, Color color, Color center) {
    const int width = std::max(3, radius / 6);
    for (int quarter = 0; quarter < 4; ++quarter) {
        // Translate hook-local coordinates to image-space positions. The woven motif can then be
        // authored around its center without repeating the offset calculation.
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

// Layer broad colored shapes and woven details into a Kazak-style carpet medallion. Keep its
// silhouette readable after mipmapping instead of relying on hairline decoration.
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
        // Translate medallion-local coordinates into the carpet image. Repeated decorative
        // strokes share the same center and orientation.
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

// Fit full-color textile artwork into a dedicated region of the floor atlas. Preserve the source
// colors rather than tinting them with the maze palette, and keep field and border regions
// separate.
void carpetArtwork(Image& atlas, const char* filename, int left, int width) {
    Image source = materialArtwork(filename);
    if (!source.data)
        return;
    ImageResize(&source, width, CarpetLayout::FieldSize);
    ImageDraw(&atlas, source, {0, 0, (float)width, (float)CarpetLayout::FieldSize},
              {(float)left, 0, (float)width, (float)CarpetLayout::FieldSize}, WHITE);
    UnloadImage(source);
}

// Create or reuse the cream, crimson, and near-black carpet atlas shared across runs. Separate
// field and border regions let the renderer place continuous runners and miter them at real wall
// corners.
Texture2D floorMaterial() {
    using namespace CarpetLayout;
    // Load or generate the fixed floor material once and retain a CPU pixel copy for later
    // uploads. The cache preserves identical carpet artwork across runs without using maze random
    // state.
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

// Generate a ceiling surface with recessed plaster coffers and corner flourishes. Upload the
// finished CPU image as an owned texture while geometry and real-time lighting remain the
// renderer's responsibility.
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

} // namespace

// Create the wall, floor, ceiling, and marble GPU materials and return them in one exclusively
// owned Textures object. A valid graphics context is required before loading or generating these
// resources.
Textures createTextures(const MazePalette& palette) {
    return {wallMaterial(palette), floorMaterial(), ceilingMaterial(palette),
            marbleMaterial(palette)};
}

// Adopt supplied material handles as exclusive ownership, or transfer them from another Textures
// object when moving. The owner releases its handles automatically; copying is disabled to
// prevent duplicate GPU deletion.
Textures::Textures(Texture2D wall, Texture2D floor, Texture2D ceiling, Texture2D marble) noexcept
    : wallTexture(wall), floorTexture(floor), ceilingTexture(ceiling), marbleTexture(marble) {}

// Unload every owned material through reset when this resource set leaves scope. The owner must
// be destroyed before CloseWindow tears down the graphics context.
Textures::~Textures() {
    reset();
}

// Adopt supplied material handles as exclusive ownership, or transfer them from another Textures
// object when moving. The owner releases its handles automatically; copying is disabled to
// prevent duplicate GPU deletion.
Textures::Textures(Textures&& other) noexcept
    : wallTexture(std::exchange(other.wallTexture, {})),
      floorTexture(std::exchange(other.floorTexture, {})),
      ceilingTexture(std::exchange(other.ceilingTexture, {})),
      marbleTexture(std::exchange(other.marbleTexture, {})) {}

// Release the destination's current materials before transferring handles from the source,
// leaving the moved-from object empty. Handle self-assignment without unloading resources that
// are still owned by this object.
Textures& Textures::operator=(Textures&& other) noexcept {
    if (this != &other) {
        reset();
        wallTexture = std::exchange(other.wallTexture, {});
        floorTexture = std::exchange(other.floorTexture, {});
        ceilingTexture = std::exchange(other.ceilingTexture, {});
        marbleTexture = std::exchange(other.marbleTexture, {});
    }
    return *this;
}

// Require nonzero handles for every material before the renderer accepts a replacement set. This
// prevents a partially created palette from displacing the currently usable resources.
bool Textures::valid() const {
    return wallTexture.id && floorTexture.id && ceilingTexture.id && marbleTexture.id;
}

// Unload any nonzero material handles and clear the stored values afterward. Destruction and move
// assignment share this cleanup path so released textures are not deleted twice.
void Textures::reset() noexcept {
    for (Texture2D* texture : {&wallTexture, &floorTexture, &ceilingTexture, &marbleTexture}) {
        if (texture->id)
            UnloadTexture(*texture);
        *texture = {};
    }
}
