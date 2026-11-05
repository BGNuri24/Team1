#include "Textures.h"
#include <initializer_list>
#include <algorithm>
#include <cmath>

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

// Paint one retro tile with a soft-edged disc, crescent, ring or arch in colors from the selected palette.
void retroTile(Image& image, int left, int top, Color base, Color motif, int pattern) {
    ImageDrawRectangle(&image,left,top,128,120,base);
    for (int y=5;y<115;++y) for (int x=7;x<121;++x) {
        const float dx=x-64.0f, dy=y-60.0f;
        const float radius=std::sqrt(dx*dx+dy*dy);
        float coverage=std::clamp(49.5f-radius,0.0f,1.0f);
        if (pattern == 1) {
            const float cut=std::sqrt((dx-26)*(dx-26)+dy*dy);
            coverage *= std::clamp(cut-40.0f,0.0f,1.0f);
        } else if (pattern == 2) {
            coverage *= std::clamp(radius-30.0f,0.0f,1.0f);
        } else if (pattern == 3 && dy > 0) {
            coverage=std::clamp(49.5f-std::fabs(dx),0.0f,1.0f);
        }
        if (coverage > 0) ImageDrawPixel(&image,left+x,top+y,ColorLerp(base,motif,coverage));
    }
}

// Combine wallpaper-sized geometric modules with painted lower paneling and a slim picture rail.
Texture2D wallMaterial(const MazePalette& palette) {
    Image image=GenImageColor(512,384,palette.wall);
    for (int row=0;row<2;++row) for (int col=0;col<4;++col) {
        const bool alternate=(row+col)%2 != 0;
        const Color base=alternate ? ColorLerp(palette.pattern,palette.wall,.22f)
            : ColorLerp(palette.wall,palette.paper,.05f);
        const Color motif=alternate ? palette.accent : ColorLerp(palette.pattern,palette.accent,.17f);
        retroTile(image,col*128,20+row*120,base,motif,(col+row*3)%4);
    }
    const Color lower=ColorLerp(palette.trim,palette.wall,.24f);
    ImageDrawRectangle(&image,0,272,512,112,lower);
    for (int x=0;x<512;x+=64) {
        frame(image,x+6,288,52,72,ColorLerp(lower,palette.paper,.09f),ColorLerp(lower,BLACK,.28f));
        ImageDrawRectangle(&image,x+10,292,44,64,ColorLerp(lower,palette.wall,.08f));
    }
    ImageDrawRectangle(&image,0,0,512,15,palette.trim);
    ImageDrawRectangle(&image,0,16,512,3,ColorLerp(palette.trim,palette.paper,.20f));
    ImageDrawRectangle(&image,0,260,512,12,palette.trim);
    ImageDrawRectangle(&image,0,261,512,2,ColorLerp(palette.accent,palette.paper,.18f));
    ImageDrawRectangle(&image,0,374,512,10,palette.trim);
    grain(image,2);
    return upload(image);
}

// Use quiet terrazzo flecks and broad inset floor tiles to balance the busier patterned walls.
Texture2D floorMaterial(const MazePalette& palette) {
    Image image=GenImageColor(SIZE,SIZE,palette.floor);
    const Color tile=ColorLerp(palette.floor,palette.wall,.13f);
    ImageDrawRectangle(&image,4,4,248,248,tile);
    frame(image,4,4,248,248,ColorLerp(tile,palette.paper,.07f),palette.floor);
    for (int y=9;y<248;y+=13) for (int x=9;x<248;x+=17) {
        const unsigned int hash=(unsigned int)(x*53+y*97);
        const Color fleck=ColorLerp(tile,hash%3 ? palette.ceiling : palette.accent,.16f);
        ImageDrawRectangle(&image,x+(int)(hash%7),y+(int)(hash%5),2+(int)(hash%2),2,fleck);
    }
    grain(image,2);
    return upload(image);
}

// Build softly colored acoustic ceiling panels with shallow seams instead of large industrial vents.
Texture2D ceilingMaterial(const MazePalette& palette) {
    Image image=GenImageColor(SIZE,SIZE,palette.ceiling);
    const Color joint=ColorLerp(palette.ceiling,palette.trim,.32f);
    frame(image,4,4,248,248,ColorLerp(palette.ceiling,palette.paper,.12f),joint);
    for (int y=18;y<245;y+=28) ImageDrawRectangle(&image,14,y,228,1,ColorLerp(palette.ceiling,joint,.23f));
    for (int y=20;y<245;y+=16) for (int x=20;x<245;x+=16)
        ImageDrawPixel(&image,x,y,ColorLerp(palette.ceiling,joint,.30f));
    grain(image,1);
    return upload(image);
}

// Center a sign label inside its atlas slot while keeping its glyphs inside the available width.
void signText(Image& image, const char* text, int center, int y, int width,
              int fontSize, Color color) {
    while (fontSize > 10 && MeasureText(text, fontSize) > width) --fontSize;
    ImageDrawText(&image, text, center-MeasureText(text, fontSize)/2, y, fontSize, color);
}

// Draw one padded sign frame inside the atlas at the supplied vertical slot offset.
void signBacking(Image& image, int top, Color accent, const MazePalette& palette) {
    ImageDrawRectangle(&image, 8, top+8, 496, 112, Fade(palette.trim,.96f));
    frame(image, 8, top+8, 496, 112, ColorLerp(palette.trim,palette.paper,.28f), palette.trim);
    ImageDrawRectangle(&image, 17, top+17, 3, 94, accent);
    ImageDrawRectangle(&image, 30, top+106, 452, 1, ColorLerp(palette.trim,palette.accent,.2f));
    for (int x : {16, 495}) for (int y : {16, 111}) {
        ImageDrawCircle(&image, x, top+y, 2, palette.accent);
    }
}

// Build the four-slot decorative sign atlas, upload it with clamped UV wrapping, and release the
// source image.
Texture2D decalMaterial(const MazePalette& palette) {
    const Color AMBER = ColorLerp(palette.accent,palette.paper,.38f);
    const Color PAPER = palette.paper;
    const Color MUTED = ColorLerp(palette.accent,palette.paper,.6f);
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

    signBacking(image, 384, MINT, palette);
    signText(image, "FIND YOUR OWN WAY", 262, 414, 444, 30, PAPER);
    signText(image, "EVERY TURN TELLS A STORY", 262, 462, 444, 16, MUTED);
    Texture2D texture = upload(image);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    return texture;
}
}

// Create the wall, floor, ceiling and decal textures and return their GPU handles as one resource
// group.
Textures createTextures(const MazePalette& palette) {
    return {wallMaterial(palette), floorMaterial(palette), ceilingMaterial(palette), decalMaterial(palette)};
}

// Unload every GPU texture owned by the resource group before its graphics context is destroyed.
void destroyTextures(Textures& textures) {
    if (textures.wall.id) UnloadTexture(textures.wall);
    if (textures.floor.id) UnloadTexture(textures.floor);
    if (textures.ceiling.id) UnloadTexture(textures.ceiling);
    if (textures.decals.id) UnloadTexture(textures.decals);
    textures = {};
}
