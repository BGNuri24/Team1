#pragma once
#include "MazePalette.h"
#include "raylib.h"

// Own one material set exclusively and release its GPU handles before the graphics context is
// closed.
class Textures {
public:
    Textures() = default;
    Textures(Texture2D wall, Texture2D floor, Texture2D ceiling, Texture2D marble) noexcept;
    ~Textures();
    Textures(const Textures&) = delete;
    Textures& operator=(const Textures&) = delete;
    Textures(Textures&& other) noexcept;
    Textures& operator=(Textures&& other) noexcept;
    // Expose borrowed handles for drawing; ownership remains in this resource set.
    const Texture2D& wall() const { return wallTexture; }
    const Texture2D& floor() const { return floorTexture; }
    const Texture2D& ceiling() const { return ceilingTexture; }
    const Texture2D& marble() const { return marbleTexture; }
    bool valid() const;

private:
    void reset() noexcept;
    Texture2D wallTexture{}, floorTexture{}, ceilingTexture{}, marbleTexture{};
};

// Generate and upload the maze materials after InitWindow has created a valid graphics context.
Textures createTextures(const MazePalette& palette = mazePalette(ActiveMazeTheme));
