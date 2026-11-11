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
void Grain(Image& CurrentImage, int Strength) {
    for (int CurrentY = 0; CurrentY < CurrentImage.height; ++CurrentY) {
        for (int CurrentX = 0; CurrentX < CurrentImage.width; ++CurrentX) {
            unsigned int Hash = (unsigned int)CurrentX * 374761393u + (unsigned int)CurrentY * 668265263u;
            Hash = (Hash ^ (Hash >> 13)) * 1274126177u;
            int Amount = (int)((Hash ^ (Hash >> 16)) % (2 * Strength + 1)) - Strength;
            Color CurrentColor = GetImageColor(CurrentImage, CurrentX, CurrentY);
            // Offset one color byte by the deterministic grain amount and clamp it to the valid
            // channel range. Applying the same adjustment to channels changes brightness without
            // unrelated hue shifts.
            auto Adjust = [Amount](unsigned char Channel) {
                int Value = Channel + Amount;
                return (unsigned char)(Value < 0 ? 0 : Value > 255 ? 255 : Value);
            };
            ImageDrawPixel(&CurrentImage, CurrentX, CurrentY, {Adjust(CurrentColor.r), Adjust(CurrentColor.g), Adjust(CurrentColor.b), 255});
        }
    }
}

// Draw light top and left edges plus dark bottom and right edges around an image-space rectangle.
// The paired colors create a shallow beveled-panel appearance in the generated texture.
void Frame(Image& CurrentImage, int CurrentX, int CurrentY, int Width, int Height, Color Light, Color Dark) {
    ImageDrawRectangle(&CurrentImage, CurrentX, CurrentY, Width, 2, Light);
    ImageDrawRectangle(&CurrentImage, CurrentX, CurrentY, 2, Height, Light);
    ImageDrawRectangle(&CurrentImage, CurrentX, CurrentY + Height - 2, Width, 2, Dark);
    ImageDrawRectangle(&CurrentImage, CurrentX + Width - 2, CurrentY, 2, Height, Dark);
}

// Upload a CPU Image to a GPU texture and release the image's pixels afterward. Configure
// mipmaps, anisotropic filtering, and repeating coordinates; the returned texture must be owned
// and unloaded exactly once.
Texture2D Upload(Image& CurrentImage) {
    Texture2D CurrentTexture = LoadTextureFromImage(CurrentImage);
    UnloadImage(CurrentImage);
    GenTextureMipmaps(&CurrentTexture);
    SetTextureFilter(CurrentTexture, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(CurrentTexture, TEXTURE_WRAP_REPEAT);
    return CurrentTexture;
}

// Approximate a cubic decorative curve with image-space strokes using the supplied control points
// and width. Ornament generation uses this without external curve assets or random draws.
void Scroll(Image& CurrentImage, Vector2 CurrentA, Vector2 B, Vector2 CurrentC, Vector2 CurrentD, int Width, Color CurrentColor) {
    Vector2 Previous = CurrentA;
    for (int Step = 1; Step <= 18; ++Step) {
        const float CurrentT = Step / 18.0f, U = 1 - CurrentT;
        Vector2 Next{U * U * U * CurrentA.x + 3 * U * U * CurrentT * B.x + 3 * U * CurrentT * CurrentT * CurrentC.x + CurrentT * CurrentT * CurrentT * CurrentD.x,
                     U * U * U * CurrentA.y + 3 * U * U * CurrentT * B.y + 3 * U * CurrentT * CurrentT * CurrentC.y + CurrentT * CurrentT * CurrentT * CurrentD.y};
        ImageDrawLineEx(&CurrentImage, Previous, Next, Width, CurrentColor);
        Previous = Next;
    }
}

// Draw a fourfold damask ornament around an image-space center, scaling its curves and stroke
// widths by size. A shadow pass beneath the thread-colored pass gives the pattern depth before
// GPU upload.
void FloralMotif(Image& CurrentImage, float CurrentX, float CurrentY, float CurrentSize, Color Thread, Color Shadow) {
    for (int CurrentLayer = 0; CurrentLayer < 2; ++CurrentLayer) {
        const Color CurrentColor = CurrentLayer ? Thread : Shadow;
        const int Width = std::max(1, (int)((CurrentLayer ? 2.1f : 4.8f) * CurrentSize));
        for (int Quarter = 0; Quarter < 4; ++Quarter) {
            // Rotate and scale a motif-local point for the current quarter-turn before
            // translating it to the image center. Four rotations reuse the same decorative curve
            // definitions.
            auto Point = [&](float Px, float Py) {
                for (int CurrentTurn = 0; CurrentTurn < Quarter; ++CurrentTurn) {
                    const float Old = Px;
                    Px = -Py;
                    Py = Old;
                }
                return Vector2{CurrentX + Px * CurrentSize, CurrentY + Py * CurrentSize};
            };
            // Transform four motif-local control points and draw their cubic scroll with the
            // current layer's width and color. The enclosing shadow and thread passes reuse this
            // curve callback.
            auto Curve = [&](Vector2 CurrentA, Vector2 B, Vector2 CurrentC, Vector2 CurrentD) {
                Scroll(CurrentImage, Point(CurrentA.x, CurrentA.y), Point(B.x, B.y), Point(CurrentC.x, CurrentC.y), Point(CurrentD.x, CurrentD.y),
                       Width, CurrentColor);
            };
            Curve({0, -4}, {-17, -18}, {-11, -34}, {0, -43});
            Curve({0, -43}, {11, -34}, {17, -18}, {0, -4});
            Curve({0, -8}, {0, -22}, {1, -29}, {0, -37});
            Curve({0, -23}, {30, -57}, {49, -22}, {25, -10});
            Curve({25, -10}, {8, -3}, {6, -31}, {22, -27});
            Curve({22, -27}, {33, -23}, {29, -15}, {24, -20});
            ImageDrawLineEx(&CurrentImage, Point(0, -57), Point(57, 0), std::max(1, Width - 1), CurrentColor);
            ImageDrawLineEx(&CurrentImage, Point(16, -42), Point(24, -43), Width, CurrentColor);
            ImageDrawLineEx(&CurrentImage, Point(40, -18), Point(43, -25), Width, CurrentColor);
        }
    }
    ImageDrawCircle(&CurrentImage, (int)CurrentX, (int)CurrentY, std::max(1, (int)(5 * CurrentSize)), Shadow);
    ImageDrawCircle(&CurrentImage, (int)CurrentX, (int)CurrentY, std::max(1, (int)(2 * CurrentSize)), Thread);
}

// Look for a material image in packaged assets beside the executable, then in src/assets for a
// development run. Return an empty Image when unavailable so material generation can use its
// procedural fallback.
Image MaterialArtwork(const char* Filename) {
    std::string Path = std::string(GetApplicationDirectory()) + "assets/" + Filename;
    if (!FileExists(Path.c_str()))
        Path = std::string("src/assets/") + Filename;
    return FileExists(Path.c_str()) ? LoadImage(Path.c_str()) : Image{};
}

// Build the wall's CPU artwork from hotel damask, stone, and layered molding details, using
// available image assets and procedural ornament. Upload the completed surface separately from
// real-time lamp lighting.
Texture2D WallMaterial(const MazePalette& CurrentPalette) {
    Image Artwork = MaterialArtwork("HotelWallPanels.png");
    if (Artwork.data) {
        ImageResize(&Artwork, 1024, 1024);
        return Upload(Artwork);
    }
    const CorridorStyle Style = GetCorridorStyle(CurrentPalette);
    Image CurrentImage = GenImageColor(1024, 1024, Style.CurrentPanel);
    ImageDrawRectangle(&CurrentImage, 0, 700, 1024, 324, Style.Trim);
    for (int CurrentX = 0; CurrentX < 1024; ++CurrentX) {
        const float Fiber = .018f + .006f * std::sin(CurrentX * .39f) + .008f * std::sin(CurrentX * .071f);
        ImageDrawRectangle(&CurrentImage, CurrentX, 0, 1, 700, ColorLerp(Style.CurrentPanel, CurrentPalette.CurrentPaper, Fiber));
        const float Wood = .03f + .025f * std::sin(CurrentX * .19f + std::sin(CurrentX * .037f));
        ImageDrawRectangle(&CurrentImage, CurrentX, 700, 1, 324, ColorLerp(Style.Trim, Style.TrimLight, Wood));
    }
    for (int CurrentLeft : {44, 532}) {
        ImageDrawRectangle(&CurrentImage, CurrentLeft, 756, 448, 190,
                           ColorLerp(Style.Trim, Style.TrimShade, .24f));
        Frame(CurrentImage, CurrentLeft, 756, 448, 190, Style.TrimShade, Style.TrimLight);
        Frame(CurrentImage, CurrentLeft + 7, 763, 434, 176, Style.TrimLight, Style.TrimShade);
        ImageDrawRectangle(&CurrentImage, CurrentLeft + 13, 769, 422, 164, Style.Trim);
    }
    Frame(CurrentImage, 62, 78, 900, 590, Style.TrimLight, Style.TrimShade);
    Frame(CurrentImage, 68, 84, 888, 578, Style.Brass, Style.TrimLight);
    for (int CurrentLeft : {0, 990}) {
        ImageDrawRectangle(&CurrentImage, CurrentLeft, 0, 34, 1024, Style.Trim);
        ImageDrawRectangle(&CurrentImage, CurrentLeft + 5, 0, 2, 1024, Style.TrimLight);
    }
    for (int CurrentY : {12, 28, 45, 675, 683, 699, 961, 984, 1009}) {
        ImageDrawRectangle(&CurrentImage, 0, CurrentY, 1024, 4, Style.TrimLight);
        ImageDrawRectangle(&CurrentImage, 0, CurrentY + 4, 1024, 3, Style.TrimShade);
    }
    Grain(CurrentImage, 1);
    return Upload(CurrentImage);
}

// Create a separate stone texture for pilaster fronts and returns so architectural pieces do not
// stretch the wall wallpaper. Return an owned GPU texture using the same filtering and cleanup
// conventions as other materials.
Texture2D MarbleMaterial(const MazePalette& CurrentPalette) {
    Image CurrentImage = MaterialArtwork("HotelMarble.png");
    if (!CurrentImage.data) {
        CurrentImage = GenImageColor(512, 512, GetCorridorStyle(CurrentPalette).Plaster);
        Grain(CurrentImage, 3);
    }
    ImageResize(&CurrentImage, 512, 512);
    return Upload(CurrentImage);
}

// Fill an image-space diamond with two triangles using independent horizontal and vertical radii.
// Carpet ornament helpers reuse this primitive for broad shapes that survive texture filtering.
void Diamond(Image& CurrentImage, int CurrentX, int CurrentY, int RadiusX, int RadiusY, Color CurrentColor) {
    const Vector2 Top{(float)CurrentX, (float)(CurrentY - RadiusY)}, Bottom{(float)CurrentX, (float)(CurrentY + RadiusY)};
    const Vector2 CurrentLeft{(float)(CurrentX - RadiusX), (float)CurrentY}, CurrentRight{(float)(CurrentX + RadiusX), (float)CurrentY};
    ImageDrawTriangle(&CurrentImage, Top, CurrentLeft, CurrentRight, CurrentColor);
    ImageDrawTriangle(&CurrentImage, CurrentLeft, Bottom, CurrentRight, CurrentColor);
}

// Fill an eight-point carpet motif with a triangle fan around its image-space center. Alternate
// outer and inner radii so the broad silhouette stays recognizable in lower mip levels.
void Star(Image& CurrentImage, int CurrentX, int CurrentY, int RadiusX, int RadiusY, Color CurrentColor) {
    // Eight broad points stay recognizable in mipmaps; tiny stitches are secondary to the shape.
    Vector2 Vertices[18]{};
    Vertices[0] = {(float)CurrentX, (float)CurrentY};
    for (int Point = 0; Point <= 16; ++Point) {
        const float Angle = Point * PI / 8.0f;
        const float Radius = Point % 2 ? .47f : 1.0f;
        Vertices[Point + 1] = {CurrentX + std::cos(Angle) * RadiusX * Radius,
                               CurrentY + std::sin(Angle) * RadiusY * Radius};
    }
    ImageDrawTriangleFan(&CurrentImage, Vertices, 18, CurrentColor);
}

// Draw an angular ram-horn motif into the carpet image using the supplied center, radius, and
// colors. The broad stepped ornament fills spaces between the larger textile medallions.
void WovenHook(Image& CurrentImage, int CurrentX, int CurrentY, int Radius, Color CurrentColor, Color CurrentCenter) {
    const int Width = std::max(3, Radius / 6);
    for (int Quarter = 0; Quarter < 4; ++Quarter) {
        // Translate hook-local coordinates to image-space positions. The woven motif can then be
        // authored around its center without repeating the offset calculation.
        auto Point = [&](int Px, int Py) {
            for (int CurrentTurn = 0; CurrentTurn < Quarter; ++CurrentTurn) {
                const int Old = Px;
                Px = -Py;
                Py = Old;
            }
            return Vector2{(float)(CurrentX + Px), (float)(CurrentY + Py)};
        };
        ImageDrawLineEx(&CurrentImage, Point(0, 0), Point(0, -Radius), Width, CurrentColor);
        ImageDrawLineEx(&CurrentImage, Point(0, -Radius), Point(Radius, -Radius), Width, CurrentColor);
        ImageDrawLineEx(&CurrentImage, Point(Radius, -Radius), Point(Radius, -Radius / 3), Width, CurrentColor);
        ImageDrawLineEx(&CurrentImage, Point(Radius, -Radius / 3), Point(Radius / 2, -Radius / 3), Width,
                        CurrentColor);
    }
    Diamond(CurrentImage, CurrentX, CurrentY, Radius / 3, Radius / 3, CurrentCenter);
}

// Layer broad colored shapes and woven details into a Kazak-style carpet medallion. Keep its
// silhouette readable after mipmapping instead of relying on hairline decoration.
void Medallion(Image& CurrentImage, int CurrentX, int CurrentY, int Radius, Color Field, Color CurrentDye, Color Yarn,
               Color Dark) {
    Diamond(CurrentImage, CurrentX, CurrentY, Radius, Radius, Dark);
    Diamond(CurrentImage, CurrentX, CurrentY, Radius - 9, Radius - 9, Yarn);
    Diamond(CurrentImage, CurrentX, CurrentY, Radius - 20, Radius - 20, CurrentDye);
    Diamond(CurrentImage, CurrentX, CurrentY, Radius - 39, Radius - 39, Dark);
    Diamond(CurrentImage, CurrentX, CurrentY, Radius - 48, Radius - 48, Yarn);
    Diamond(CurrentImage, CurrentX, CurrentY, Radius - 61, Radius - 61, CurrentDye);
    Diamond(CurrentImage, CurrentX, CurrentY, (int)(Radius * .60f), (int)(Radius * .60f), Yarn);
    Diamond(CurrentImage, CurrentX, CurrentY, (int)(Radius * .55f), (int)(Radius * .55f), Field);
    Diamond(CurrentImage, CurrentX, CurrentY, (int)(Radius * .43f), (int)(Radius * .43f), CurrentDye);
    Star(CurrentImage, CurrentX, CurrentY, (int)(Radius * .29f), (int)(Radius * .29f), Yarn);
    Star(CurrentImage, CurrentX, CurrentY, (int)(Radius * .21f), (int)(Radius * .21f), Dark);
    Diamond(CurrentImage, CurrentX, CurrentY, (int)(Radius * .09f), (int)(Radius * .09f), Yarn);
    for (int CurrentHorizontal : {-1, 1})
        for (int Vertical : {-1, 1}) {
            WovenHook(CurrentImage, CurrentX + CurrentHorizontal * Radius / 3, CurrentY + Vertical * Radius / 3, 14, Yarn,
                      Dark);
            Diamond(CurrentImage, CurrentX + CurrentHorizontal * Radius / 6, CurrentY + Vertical * Radius / 6, 12, 12, Field);
        }
    for (int Quarter = 0; Quarter < 4; ++Quarter) {
        // Translate medallion-local coordinates into the carpet image. Repeated decorative
        // strokes share the same center and orientation.
        auto Point = [&](int Px, int Py) {
            for (int CurrentTurn = 0; CurrentTurn < Quarter; ++CurrentTurn) {
                const int Old = Px;
                Px = -Py;
                Py = Old;
            }
            return Vector2{(float)(CurrentX + Px), (float)(CurrentY + Py)};
        };
        // Stepped hooks and opposing stitches echo the woven border around the reference diamonds.
        for (int Step = 0; Step < 7; ++Step) {
            const int CurrentA = 38 + Step * (Radius - 90) / 7, B = Radius - 26 - CurrentA;
            const Vector2 Stitch = Point(CurrentA, -B);
            Diamond(CurrentImage, (int)Stitch.x, (int)Stitch.y, 7, 7, Yarn);
            const Vector2 Tip = Point(CurrentA - 8, -B - 22);
            ImageDrawLineEx(&CurrentImage, Point(CurrentA - 8, -B - 4), Tip, 5, Yarn);
            ImageDrawLineEx(&CurrentImage, Tip, Point(CurrentA + 5, -B - 22), 5, Yarn);
        }
        for (int Step = 0; Step < 5; ++Step) {
            const int CurrentA = 25 + Step * (Radius / 2 - 40) / 5;
            const Vector2 Stitch = Point(CurrentA, -(int)(Radius * .58f) + CurrentA);
            Diamond(CurrentImage, (int)Stitch.x, (int)Stitch.y, 5, 5, Dark);
        }
    }
}

// Fit full-color textile artwork into a dedicated region of the floor atlas. Preserve the source
// colors rather than tinting them with the maze palette, and keep field and border regions
// separate.
void CarpetArtwork(Image& Atlas, const char* Filename, int CurrentLeft, int Width) {
    Image Source = MaterialArtwork(Filename);
    if (!Source.data)
        return;
    ImageResize(&Source, Width, CarpetLayout::FieldSize);
    ImageDraw(&Atlas, Source, {0, 0, (float)Width, (float)CarpetLayout::FieldSize},
              {(float)CurrentLeft, 0, (float)Width, (float)CarpetLayout::FieldSize}, WHITE);
    UnloadImage(Source);
}

// Create or reuse the cream, crimson, and near-black carpet atlas shared across runs. Separate
// field and border regions let the renderer place continuous runners and miter them at real wall
// corners.
Texture2D FloorMaterial() {
    using namespace CarpetLayout;
    // Load or generate the fixed floor material once and retain a CPU pixel copy for later
    // uploads. The cache preserves identical carpet artwork across runs without using maze random
    // state.
    static const std::vector<Color> Pixels = [] {
        constexpr Color Field{214, 199, 164, 255}, CurrentDye{165, 38, 29, 255};
        constexpr Color Yarn{225, 207, 165, 255}, Dark{33, 24, 21, 255};
        // Procedural motifs keep the material usable if the packaged artwork is unavailable.
        Image CurrentImage = GenImageColor(AtlasWidth, FieldSize, Field);
        Medallion(CurrentImage, 512, 512, 332, Field, CurrentDye, Yarn, Dark);
        for (int CurrentX : {238, 786})
            for (int CurrentY : {176, 848}) {
                Diamond(CurrentImage, CurrentX, CurrentY, 65, 65, CurrentDye);
                Diamond(CurrentImage, CurrentX, CurrentY, 54, 54, Yarn);
                Diamond(CurrentImage, CurrentX, CurrentY, 44, 44, Dark);
                Star(CurrentImage, CurrentX, CurrentY, 34, 34, Yarn);
                Diamond(CurrentImage, CurrentX, CurrentY, 13, 13, CurrentDye);
            }
        for (int CurrentY : {0, 1024}) {
            Star(CurrentImage, 512, CurrentY, 73, 73, Yarn);
            Star(CurrentImage, 512, CurrentY, 52, 52, CurrentDye);
        }
        for (int CurrentX : {190, 834})
            for (int CurrentY = 320; CurrentY <= 704; CurrentY += 96) {
                Star(CurrentImage, CurrentX, CurrentY, 31, 31, CurrentDye);
                Star(CurrentImage, CurrentX, CurrentY, 20, 20, Yarn);
            }
        for (int CurrentX : {350, 674})
            for (int CurrentY : {88, 936}) {
                WovenHook(CurrentImage, CurrentX, CurrentY, 32, Yarn, CurrentDye);
                Diamond(CurrentImage, CurrentX, CurrentY, 10, 10, CurrentDye);
            }
        for (int CurrentY : {256, 768})
            for (int CurrentX : {190, 834})
                WovenHook(CurrentImage, CurrentX, CurrentY, 23, ColorLerp(Yarn, CurrentDye, .28f), CurrentDye);
        for (int CurrentY : {115, 909}) {
            Diamond(CurrentImage, 512, CurrentY, 37, 37, CurrentDye);
            Diamond(CurrentImage, 512, CurrentY, 27, 27, Yarn);
            Star(CurrentImage, 512, CurrentY, 18, 18, Field);
        }
        // A broad repeating star band sits between narrow guard stripes and a stepped meander.
        // The dark guard gap prevents border colors bleeding into the field's mipmapped repeat
        // edge.
        const int CurrentBorder = BorderOffset;
        ImageDrawRectangle(&CurrentImage, CurrentBorder, 0, BorderSize, FieldSize, CurrentDye);
        for (int CurrentOffset : {0, 19, 40, 178, 192, 232, 247})
            ImageDrawRectangle(&CurrentImage, CurrentBorder + CurrentOffset, 0, CurrentOffset == 192 ? 40 : 6, FieldSize,
                               CurrentOffset == 192 ? Yarn : Dark);
        for (int CurrentY = 64; CurrentY < FieldSize; CurrentY += 128) {
            Star(CurrentImage, CurrentBorder + 109, CurrentY, 65, 50, Yarn);
            Star(CurrentImage, CurrentBorder + 109, CurrentY, 46, 35, Dark);
            Diamond(CurrentImage, CurrentBorder + 109, CurrentY, 24, 19, CurrentDye);
            for (int End : {-1, 1})
                Diamond(CurrentImage, CurrentBorder + 109 + End * 52, CurrentY + 56, 16, 12, Yarn);
        }
        for (int CurrentY = -32; CurrentY < FieldSize; CurrentY += 64) {
            ImageDrawRectangle(&CurrentImage, CurrentBorder + 202, CurrentY, 20, 8, CurrentDye);
            ImageDrawRectangle(&CurrentImage, CurrentBorder + 202, CurrentY, 7, 36, CurrentDye);
            ImageDrawRectangle(&CurrentImage, CurrentBorder + 202, CurrentY + 29, 20, 7, CurrentDye);
            ImageDrawRectangle(&CurrentImage, CurrentBorder + 215, CurrentY + 29, 7, 35, CurrentDye);
        }
        CarpetArtwork(CurrentImage, "CarpetPersianField.png", 0, FieldSize);
        CarpetArtwork(CurrentImage, "CarpetPersianBorder.png", BorderOffset, BorderSize);
        // Low-contrast weave preserves large filled motifs and lets mipmaps remove subpixel
        // threads.
        for (int CurrentY = 0; CurrentY < CurrentImage.height; ++CurrentY)
            for (int CurrentX = 0; CurrentX < BorderOffset + BorderSize; ++CurrentX) {
                const Color CurrentColor = GetImageColor(CurrentImage, CurrentX, CurrentY);
                ImageDrawPixel(&CurrentImage, CurrentX, CurrentY, ColorLerp(CurrentColor, (CurrentX + CurrentY) % 2 ? Dark : Field, .035f));
            }
        Grain(CurrentImage, 2);
        Color* CurrentColors = LoadImageColors(CurrentImage);
        std::vector<Color> Result(CurrentColors, CurrentColors + AtlasWidth * FieldSize);
        UnloadImageColors(CurrentColors);
        UnloadImage(CurrentImage);
        return Result;
    }();
    Image CurrentImage = GenImageColor(AtlasWidth, FieldSize, BLANK);
    std::copy(Pixels.begin(), Pixels.end(), static_cast<Color*>(CurrentImage.data));
    return Upload(CurrentImage);
}

// Generate a ceiling surface with recessed plaster coffers and corner flourishes. Upload the
// finished CPU image as an owned texture while geometry and real-time lighting remain the
// renderer's responsibility.
Texture2D CeilingMaterial(const MazePalette& CurrentPalette) {
    const CorridorStyle Style = GetCorridorStyle(CurrentPalette);
    Image CurrentImage = GenImageColor(512, 512, Style.Ceiling);
    ImageDrawRectangle(&CurrentImage, 43, 43, 426, 426, Style.CeilingInset);
    for (int Inset : {10, 22, 37, 47})
        Frame(CurrentImage, Inset, Inset, 512 - Inset * 2, 512 - Inset * 2,
              ColorLerp(Style.Ceiling, CurrentPalette.CurrentPaper, .20f),
              ColorLerp(Style.Ceiling, Style.TrimShade, .24f));
    for (int CurrentX : {88, 424})
        for (int CurrentY : {88, 424})
            FloralMotif(CurrentImage, (float)CurrentX, (float)CurrentY, .43f,
                        ColorLerp(Style.CeilingInset, Style.Trim, .22f), Style.CeilingInset);
    Grain(CurrentImage, 1);
    return Upload(CurrentImage);
}

} // namespace

// Create the wall, floor, ceiling, and marble GPU materials and return them in one exclusively
// owned Textures object. A valid graphics context is required before loading or generating these
// resources.
Textures CreateTextures(const MazePalette& CurrentPalette) {
    return {WallMaterial(CurrentPalette), FloorMaterial(), CeilingMaterial(CurrentPalette),
            MarbleMaterial(CurrentPalette)};
}

// Adopt supplied material handles as exclusive ownership, or transfer them from another Textures
// object when moving. The owner releases its handles automatically; copying is disabled to
// prevent duplicate GPU deletion.
Textures::Textures(Texture2D CurrentWall, Texture2D CurrentFloor, Texture2D Ceiling, Texture2D Marble) noexcept
    : WallTexture(CurrentWall), FloorTexture(CurrentFloor), CeilingTexture(Ceiling), MarbleTexture(Marble) {}

// Unload every owned material through reset when this resource set leaves scope. The owner must
// be destroyed before CloseWindow tears down the graphics context.
Textures::~Textures() {
    Reset();
}

// Adopt supplied material handles as exclusive ownership, or transfer them from another Textures
// object when moving. The owner releases its handles automatically; copying is disabled to
// prevent duplicate GPU deletion.
Textures::Textures(Textures&& CurrentOther) noexcept
    : WallTexture(std::exchange(CurrentOther.WallTexture, {})),
      FloorTexture(std::exchange(CurrentOther.FloorTexture, {})),
      CeilingTexture(std::exchange(CurrentOther.CeilingTexture, {})),
      MarbleTexture(std::exchange(CurrentOther.MarbleTexture, {})) {}

// Release the destination's current materials before transferring handles from the source,
// leaving the moved-from object empty. Handle self-assignment without unloading resources that
// are still owned by this object.
Textures& Textures::operator=(Textures&& CurrentOther) noexcept {
    if (this != &CurrentOther) {
        Reset();
        WallTexture = std::exchange(CurrentOther.WallTexture, {});
        FloorTexture = std::exchange(CurrentOther.FloorTexture, {});
        CeilingTexture = std::exchange(CurrentOther.CeilingTexture, {});
        MarbleTexture = std::exchange(CurrentOther.MarbleTexture, {});
    }
    return *this;
}

// Require nonzero handles for every material before the renderer accepts a replacement set. This
// prevents a partially created palette from displacing the currently usable resources.
bool Textures::Valid() const {
    return WallTexture.id && FloorTexture.id && CeilingTexture.id && MarbleTexture.id;
}

// Unload any nonzero material handles and clear the stored values afterward. Destruction and move
// assignment share this cleanup path so released textures are not deleted twice.
void Textures::Reset() noexcept {
    for (Texture2D* CurrentTexture : {&WallTexture, &FloorTexture, &CeilingTexture, &MarbleTexture}) {
        if (CurrentTexture->id)
            UnloadTexture(*CurrentTexture);
        *CurrentTexture = {};
    }
}
