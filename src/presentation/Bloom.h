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
    bool Begin(bool LinearHdr = false);
    // Capture the actual 3D view and projection matrices after BeginMode3D and enable surface
    // effects only when their buffers and shader exist. Flush batches and disable blending while
    // material data is written to attachments.
    void SetView(Matrix View, Matrix Perspective);
    // Finish scene capture, extract and blur highlights, optionally compute screen-space surface
    // effects, and composite into the caller's saved framebuffer. Restore matrices and drawing
    // state so later HUD and UI rendering remains unaffected.
    void End();

private:
    // Rebuild scene and intermediate targets when the requested dimensions change, reusing the
    // current targets otherwise. Clean up incomplete allocations and remember attempted
    // dimensions so failures are not retried every frame at the same size.
    bool Resize(int Width, int Height);
    Shader CurrentShader{}, CurrentComposite{}, SurfaceShader{};
    RenderTexture2D CurrentScene{}, CurrentHorizontal{}, Vertical{}, CurrentSurfaceEffects{};
    Texture2D CurrentGeometry{};
    int StepLocation = -1, ExtractLocation = -1;
    int HdrLocation = -1, CompositeHdrLocation = -1, BloomLocation = -1;
    int DepthLocation = -1, GeometryLocation = -1;
    int ViewLocation = -1, PerspectiveLocation = -1, InversePerspectiveLocation = -1;
    int EffectsLocation = -1, EffectsEnabledLocation = -1, CompositeDepthLocation = -1;
    int CompositeInverseLocation = -1;
    bool UseLinearHdr = false;
    bool UseSurfaceEffects = false;
    Matrix SurfaceView{}, SurfacePerspective{}, InversePerspective{};
    int AttemptedWidth = 0, AttemptedHeight = 0;
    unsigned int Destination = 0;
    int DestinationWidth = 0, DestinationHeight = 0;
    Matrix Projection{}, Modelview{};
};
