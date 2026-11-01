#include "Textures.h"
#include <initializer_list>

namespace {
constexpr int SIZE = 256;

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

// Draw light top/left edges and dark bottom/right edges to give a rectangular texture panel a beveled
// appearance.
void frame(Image& image, int x, int y, int width, int height, Color light, Color dark) {
    ImageDrawRectangle(&image, x, y, width, 2, light);
    ImageDrawRectangle(&image, x, y, 2, height, light);
    ImageDrawRectangle(&image, x, y + height - 2, width, 2, dark);
    ImageDrawRectangle(&image, x + width - 2, y, 2, height, dark);
}

// Upload the image and free its CPU pixels, then enable mipmaps, anisotropic filtering and repeating
// texture coordinates.
Texture2D upload(Image& image) {
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    return texture;
}

// Build the engraved teal wall panel with recessed maze motifs, fasteners and brass detailing.
Texture2D wallMaterial() {
    Image image = GenImageColor(SIZE, SIZE, {23, 43, 49, 255});
    ImageDrawRectangle(&image, 12, 12, 232, 232, {51, 86, 91, 255});
    frame(image, 12, 12, 232, 232, {79, 116, 117, 255}, {14, 31, 37, 255});
    ImageDrawRectangle(&image, 25, 29, 206, 189, {43, 75, 81, 255});
    frame(image, 25, 29, 206, 189, {28, 54, 61, 255}, {63, 98, 102, 255});
    // Draw nested recessed lines into the wall material as decoration independent of the generated maze
    // layout.
    for (int i = 0; i < 4; ++i) {
        int x = 47 + i * 23, y = 55 + i * 23;
        ImageDrawRectangle(&image, x, y, 164 - i * 23, 3, {28, 55, 62, 255});
        ImageDrawRectangle(&image, x, y, 3, 133 - i * 23, {28, 55, 62, 255});
        ImageDrawRectangle(&image, x + 3, y + 3, 161 - i * 23, 1, {65, 97, 101, 255});
    }
    ImageDrawRectangle(&image, 46, 188, 119, 3, {28, 55, 62, 255});
    ImageDrawRectangle(&image, 164, 147, 3, 44, {28, 55, 62, 255});
    ImageDrawRectangle(&image, 168, 147, 40, 3, {28, 55, 62, 255});
    for (int x : {19, 235}) for (int y : {19, 235}) {
        ImageDrawCircle(&image, x, y, 3, {14, 31, 37, 255});
        ImageDrawPixel(&image, x - 1, y - 1, {137, 141, 122, 255});
    }
    // Add a narrow brass seam along the lower edge of the wall texture before applying surface grain.
    ImageDrawRectangle(&image, 37, 233, 182, 2, {122, 105, 70, 255});
    grain(image, 3);
    return upload(image);
}

// Build a dark plated floor texture with panel grooves and warm corner inlays.
Texture2D floorMaterial() {
    Image image = GenImageColor(SIZE, SIZE, {16, 28, 34, 255});
    ImageDrawRectangle(&image, 7, 7, 242, 242, {53, 67, 70, 255});
    frame(image, 7, 7, 242, 242, {73, 88, 88, 255}, {21, 37, 43, 255});
    frame(image, 23, 23, 210, 210, {33, 48, 53, 255}, {64, 79, 80, 255});
    for (int y = 42; y < 220; y += 8)
        ImageDrawRectangle(&image, 38, y, 180, 1, {47, 61, 65, 255});
    // Mark the floor panel's corners with warm inlays to distinguish its edges without outlining the
    // entire tile.
    for (int x : {15, 226}) for (int y : {15, 226}) {
        ImageDrawRectangle(&image, x, y, 15, 2, {146, 125, 83, 255});
        ImageDrawRectangle(&image, x, y, 2, 15, {146, 125, 83, 255});
    }
    grain(image, 3);
    return upload(image);
}

// Build a vented ceiling panel texture with contrasting seams and repeated grille lines.
Texture2D ceilingMaterial() {
    Image image = GenImageColor(SIZE, SIZE, {21, 37, 45, 255});
    ImageDrawRectangle(&image, 8, 8, 240, 240, {36, 58, 65, 255});
    frame(image, 8, 8, 240, 240, {56, 79, 83, 255}, {15, 29, 37, 255});
    ImageDrawRectangle(&image, 20, 108, 216, 40, {20, 38, 46, 255});
    for (int x = 29; x < 230; x += 12)
        ImageDrawRectangle(&image, x, 114, 3, 28, {55, 77, 82, 255});
    frame(image, 24, 25, 208, 54, {44, 67, 74, 255}, {25, 44, 52, 255});
    frame(image, 24, 177, 208, 54, {44, 67, 74, 255}, {25, 44, 52, 255});
    grain(image, 2);
    return upload(image);
}

void signText(Image& image, const char* text, int center, int y, int width,
              int fontSize, Color color) {
    while (fontSize > 10 && MeasureText(text, fontSize) > width) --fontSize;
    ImageDrawText(&image, text, center-MeasureText(text, fontSize)/2, y, fontSize, color);
}

// Draw one padded sign frame inside the atlas at the supplied vertical slot offset.
void signBacking(Image& image, int top, Color accent) {
    ImageDrawRectangle(&image, 8, top+8, 496, 112, {10, 24, 31, 238});
    frame(image, 8, top+8, 496, 112, {77, 101, 103, 255}, {22, 46, 54, 255});
    ImageDrawRectangle(&image, 17, top+17, 3, 94, accent);
    ImageDrawRectangle(&image, 30, top+106, 452, 1, {44, 68, 74, 255});
    for (int x : {16, 495}) for (int y : {16, 111}) {
        ImageDrawCircle(&image, x, top+y, 2, {139, 140, 119, 255});
    }
}

// Build the four-slot decorative sign atlas, upload it with clamped UV wrapping, and release the
// source image.
Texture2D decalMaterial() {
    constexpr Color AMBER = {244, 190, 102, 255};
    constexpr Color PAPER = {238, 233, 215, 255};
    constexpr Color MUTED = {148, 174, 177, 255};
    constexpr Color MINT = {113, 216, 182, 255};
    Image image = GenImageColor(512, 512, BLANK);

    signBacking(image, 0, AMBER);
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

    signBacking(image, 128, MINT);
    signText(image, "07", 69, 169, 70, 40, MINT);
    ImageDrawRectangle(&image, 116, 155, 1, 73, {54, 83, 87, 255});
    signText(image, "SECTOR", 303, 159, 334, 34, PAPER);
    signText(image, "SURVEY ZONE", 303, 206, 334, 16, MUTED);

    signBacking(image, 256, AMBER);
    for (int i = 0; i < 4; ++i) {
        // Add repeated diagonal maintenance markings to the sign; their orientation has no navigation
        // meaning.
        int y = 286 + i*16;
        ImageDrawLine(&image, 34, y+9, 49, y-6, AMBER);
        ImageDrawLine(&image, 35, y+9, 50, y-6, AMBER);
        ImageDrawLine(&image, 463, y+9, 478, y-6, AMBER);
        ImageDrawLine(&image, 464, y+9, 479, y-6, AMBER);
    }
    signText(image, "MAINTENANCE", 256, 287, 390, 32, PAPER);
    signText(image, "KEEP ACCESS CLEAR", 256, 334, 390, 16, AMBER);

    signBacking(image, 384, MINT);
    signText(image, "FIND YOUR OWN WAY", 262, 414, 444, 30, PAPER);
    signText(image, "EVERY TURN TELLS A STORY", 262, 462, 444, 16, MUTED);
    Texture2D texture = upload(image);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    return texture;
}
}

// Create the wall, floor, ceiling and decal textures and return their GPU handles as one resource
// group.
Textures createTextures() {
    return {wallMaterial(), floorMaterial(), ceilingMaterial(), decalMaterial()};
}

// Unload every GPU texture owned by the resource group before its graphics context is destroyed.
void destroyTextures(Textures& textures) {
    UnloadTexture(textures.wall);
    UnloadTexture(textures.floor);
    UnloadTexture(textures.ceiling);
    UnloadTexture(textures.decals);
}
