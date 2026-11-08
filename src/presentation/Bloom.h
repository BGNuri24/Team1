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
    // Supply the actual 3D matrices after BeginMode3D; ordinary 2D bloom stays unchanged.
    void setView(Matrix view, Matrix perspective);
    void end();

private:
    bool resize(int width, int height);
    Shader shader{}, composite{}, surfaceShader{};
    RenderTexture2D scene{}, horizontal{}, vertical{}, surfaceEffects{};
    Texture2D geometry{};
    int stepLocation = -1, extractLocation = -1;
    int hdrLocation = -1, compositeHdrLocation = -1, bloomLocation = -1;
    int depthLocation = -1, geometryLocation = -1;
    int viewLocation = -1, perspectiveLocation = -1, inversePerspectiveLocation = -1;
    int effectsLocation = -1, effectsEnabledLocation = -1, compositeDepthLocation = -1;
    int compositeInverseLocation = -1;
    bool useLinearHdr = false;
    bool useSurfaceEffects = false;
    Matrix surfaceView{}, surfacePerspective{}, inversePerspective{};
    int attemptedWidth = 0, attemptedHeight = 0;
    unsigned int destination = 0;
    int destinationWidth = 0, destinationHeight = 0;
    Matrix projection{}, modelview{};
};
