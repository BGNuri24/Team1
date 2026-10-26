#include "Textures.h"
#include <initializer_list>

namespace {
constexpr int SIZE = 256;

// Deterministic surface grain: creating materials never changes maze randomness.
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

void frame(Image& image, int x, int y, int width, int height, Color light, Color dark) {
    ImageDrawRectangle(&image, x, y, width, 2, light);
    ImageDrawRectangle(&image, x, y, 2, height, light);
    ImageDrawRectangle(&image, x, y + height - 2, width, 2, dark);
    ImageDrawRectangle(&image, x + width - 2, y, 2, height, dark);
}

Texture2D upload(Image& image) {
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    return texture;
}

Texture2D wallMaterial() {
    Image image = GenImageColor(SIZE, SIZE, {23, 43, 49, 255});
    ImageDrawRectangle(&image, 12, 12, 232, 232, {51, 86, 91, 255});
    frame(image, 12, 12, 232, 232, {79, 116, 117, 255}, {14, 31, 37, 255});
    ImageDrawRectangle(&image, 25, 29, 206, 189, {43, 75, 81, 255});
    frame(image, 25, 29, 206, 189, {28, 54, 61, 255}, {63, 98, 102, 255});
    // Recessed architectural maze motif. It is a material, not a route hint.
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
    // Fine brass seams at the foot of each panel.
    ImageDrawRectangle(&image, 37, 233, 182, 2, {122, 105, 70, 255});
    grain(image, 3);
    return upload(image);
}

Texture2D floorMaterial() {
    Image image = GenImageColor(SIZE, SIZE, {16, 28, 34, 255});
    ImageDrawRectangle(&image, 7, 7, 242, 242, {53, 67, 70, 255});
    frame(image, 7, 7, 242, 242, {73, 88, 88, 255}, {21, 37, 43, 255});
    frame(image, 23, 23, 210, 210, {33, 48, 53, 255}, {64, 79, 80, 255});
    for (int y = 42; y < 220; y += 8)
        ImageDrawRectangle(&image, 38, y, 180, 1, {47, 61, 65, 255});
    // Small warm corner inlays keep the floor readable without a bright grid.
    for (int x : {15, 226}) for (int y : {15, 226}) {
        ImageDrawRectangle(&image, x, y, 15, 2, {146, 125, 83, 255});
        ImageDrawRectangle(&image, x, y, 2, 15, {146, 125, 83, 255});
    }
    grain(image, 3);
    return upload(image);
}

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
}

Textures createTextures() {
    return {wallMaterial(), floorMaterial(), ceilingMaterial()};
}

void destroyTextures(Textures& textures) {
    UnloadTexture(textures.wall);
    UnloadTexture(textures.floor);
    UnloadTexture(textures.ceiling);
}
