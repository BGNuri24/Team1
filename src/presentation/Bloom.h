#pragma once
#include "raylib.h"

// Own the world's HDR lighting, glow and display conversion. Targets are reused and resized;
// unsupported shaders or failed allocations leave the normal renderer available.
class Bloom {
public:
    Bloom();
    ~Bloom();
    Bloom(const Bloom&) = delete;
    Bloom& operator=(const Bloom&) = delete;
    bool begin(bool linearHdr = false);
    void end();

private:
    bool resize(int width, int height);
    Shader shader{}, composite{};
    RenderTexture2D scene{}, horizontal{}, vertical{};
    int stepLocation = -1, extractLocation = -1;
    int hdrLocation = -1, compositeHdrLocation = -1, bloomLocation = -1;
    bool useLinearHdr = false;
    int attemptedWidth = 0, attemptedHeight = 0;
    unsigned int destination = 0;
    int destinationWidth = 0, destinationHeight = 0;
    Matrix projection{}, modelview{};
};
