#include "Textures.h"

static unsigned char clampByte(int v) {
    return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// the same color, brighter (d > 0) or darker (d < 0)
static Color shade(Color c, int d) {
    return Color{ clampByte(c.r + d), clampByte(c.g + d), clampByte(c.b + d), 255 };
}

// random grain on every pixel, so surfaces don't look flat
static void addNoise(Image& img, int amount) {
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            Color c = GetImageColor(img, x, y);
            ImageDrawPixel(&img, x, y, shade(c, GetRandomValue(-amount, amount)));
        }
}

// image -> GPU texture with smooth filtering at a distance and at steep angles
static Texture2D toTexture(Image& img) {
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    SetTextureFilter(t, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}

// red bricks with light mortar, 4 bricks across and 8 rows
static Texture2D makeWallTexture() {
    const int S = 256, brickW = 64, brickH = 32, gap = 3;
    // mortar
    Image img = GenImageColor(S, S, Color{ 168, 160, 150, 255 });          

    for (int row = 0; row < S / brickH; row++) {
        // every second row is shifted
        int offset = (row % 2) * (brickW / 2);                           
        for (int col = 0; col < S / brickW; col++) {
            int x = col * brickW + offset + gap / 2;
            int y = row * brickH + gap / 2;
            int w = brickW - gap, h = brickH - gap;

            // each brick has its own tone
            int v = GetRandomValue(-22, 22);                             
            Color base = { clampByte(150 + v), clampByte(74 + v / 2), clampByte(58 + v / 2), 255 };
            
            // 2nd copy wraps around the right edge
            for (int wrap = 0; wrap < 2; wrap++) {                       
                int px = x - wrap * S;
                ImageDrawRectangle(&img, px, y, w, h, base);
                // light top edge
                ImageDrawRectangle(&img, px, y, w, 2, shade(base, 25));            
                // dark bottom edge
                ImageDrawRectangle(&img, px, y + h - 2, w, 2, shade(base, -30));   
            }
        }
    }
    addNoise(img, 10);
    return toTexture(img);
}

// gray stone tiles with dark grout
static Texture2D makeFloorTexture() {
    const int S = 256, tile = 64, gap = 3;
    // grout
    Image img = GenImageColor(S, S, Color{ 40, 40, 44, 255 });             

    for (int row = 0; row < S / tile; row++)
        for (int col = 0; col < S / tile; col++) {
            int v = GetRandomValue(-14, 14);
            Color base = { clampByte(104 + v), clampByte(102 + v), clampByte(100 + v), 255 };
            ImageDrawRectangle(&img, col * tile + gap / 2, row * tile + gap / 2, tile - gap, tile - gap, base);
        }
    addNoise(img, 12);
    return toTexture(img);
}

Textures createTextures() {
    Textures t;
    t.wall = makeWallTexture();
    t.floor = makeFloorTexture();
    return t;
}

void destroyTextures(Textures& t) {
    UnloadTexture(t.wall);
    UnloadTexture(t.floor);
}