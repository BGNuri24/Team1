#pragma once
#include "raylib.h"

// Own the world-only glow pass. Targets are reused between frames and replaced on resize;
// unsupported shaders or failed allocations leave the normal renderer available.
class Bloom {
public:
    Bloom();
    ~Bloom();
    Bloom(const Bloom&) = delete;
    Bloom& operator=(const Bloom&) = delete;
    bool begin();
    void end();

private:
    bool resize(int width, int height);
    Shader shader{};
    RenderTexture2D scene{}, horizontal{}, vertical{};
    int stepLocation = -1, extractLocation = -1;
    int attemptedWidth = 0, attemptedHeight = 0;
    unsigned int destination = 0;
    int destinationWidth = 0, destinationHeight = 0;
    Matrix projection{}, modelview{};
};
