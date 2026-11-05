#pragma once
#include "MazePalette.h"
#include "raylib.h"

// Own one material set exclusively and release its GPU handles before the graphics context is
// closed.
class Textures {
public:
    Textures() = default;
    Textures(Texture2D wall, Texture2D floor, Texture2D ceiling, Texture2D decals) noexcept;
    ~Textures();
    Textures(const Textures&) = delete;
    Textures& operator=(const Textures&) = delete;
    Textures(Textures&& other) noexcept;
    Textures& operator=(Textures&& other) noexcept;
    // Expose borrowed handles for drawing; ownership remains in this resource set.
    const Texture2D& wall() const { return wallTexture; }
    const Texture2D& floor() const { return floorTexture; }
    const Texture2D& ceiling() const { return ceilingTexture; }
    const Texture2D& decals() const { return decalTexture; }
    bool valid() const;

private:
    void reset() noexcept;
    Texture2D wallTexture{}, floorTexture{}, ceilingTexture{}, decalTexture{};
};

// Generate and upload the maze materials after InitWindow has created a valid graphics context.
Textures createTextures(const MazePalette& palette = mazePalette(MazeTheme::Slate));
