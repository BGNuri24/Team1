#pragma once
#include "raylib.h"

// Own the world's HDR lighting, glow and display conversion. Targets are reused and resized;
// unsupported shaders or failed allocations leave the normal renderer available.
class Bloom {
public:
    // Compile highlight-blur, composition, and surface-effect shaders and cache the uniforms
    // required by successful custom programs. Leave unavailable shaders empty so the caller can
    // use normal world rendering.
    Bloom();
    // Release scene and intermediate render targets, the separately owned geometry attachment,
    // and successfully loaded shaders. These GPU resources must be destroyed while the graphics
    // context still exists.
    ~Bloom();
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    Bloom(const Bloom&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    Bloom& operator=(const Bloom&) = delete;
    // Prepare targets for the current output size, save the caller's framebuffer and matrices,
    // and begin scene capture. Return false when effects are unavailable so rendering can
    // continue directly to the original destination.
    bool begin(bool linearHdr = false);
    // Capture the actual 3D view and projection matrices after BeginMode3D and enable surface
    // effects only when their buffers and shader exist. Flush batches and disable blending while
    // material data is written to attachments.
    void setView(Matrix view, Matrix perspective);
    // Finish scene capture, extract and blur highlights, optionally compute screen-space surface
    // effects, and composite into the caller's saved framebuffer. Restore matrices and drawing
    // state so later HUD and UI rendering remains unaffected.
    void end();

private:
    // Rebuild scene and intermediate targets when the requested dimensions change, reusing the
    // current targets otherwise. Clean up incomplete allocations and remember attempted
    // dimensions so failures are not retried every frame at the same size.
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
