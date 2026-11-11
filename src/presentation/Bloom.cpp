#include "Bloom.h"
#include "MazeShaders.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>

namespace {
// Require framebuffer, color, and depth handles and confirm framebuffer completeness before using
// a render target. Invalid targets cause post-processing to fall back rather than draw into an
// incomplete framebuffer.
bool Valid(RenderTexture2D Target) {
    return Target.id && Target.texture.id && Target.depth.id && rlFramebufferComplete(Target.id);
}

// Unload a render target when it owns a framebuffer, then clear every stored handle. Resetting
// the value lets resize and destruction share cleanup without reusing released GPU resources.
void Release(RenderTexture2D& Target) {
    if (Target.id)
        UnloadRenderTexture(Target);
    Target = {};
}

// Draw a source texture across the requested pixel dimensions with an optional tint. Use a
// negative source height to correct render-texture orientation during fullscreen post-processing
// passes.
void DrawTarget(Texture2D Source, int Width, int Height, Color Tint = WHITE) {
    DrawTexturePro(Source, {0, 0, (float)Source.width, -(float)Source.height},
                   {0, 0, (float)Width, (float)Height}, {0, 0}, 0, Tint);
}

// Create a render target whose color attachment stores floating-point RGBA16F light energy above
// one. Retain the normal depth-buffer ownership and release incomplete allocations before
// reporting failure.
RenderTexture2D HdrTarget(int Width, int Height) {
    RenderTexture2D Target = LoadRenderTexture(Width, Height);
    if (!Target.id)
        return {};
    const unsigned int CurrentColor =
        rlLoadTexture(nullptr, Width, Height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    if (!CurrentColor) {
        UnloadRenderTexture(Target);
        return {};
    }
    const Texture2D Previous = Target.texture;
    Target.texture = {CurrentColor, Width, Height, 1, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16};
    rlFramebufferAttach(Target.id, CurrentColor, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    UnloadTexture(Previous);
    if (!Valid(Target)) {
        UnloadRenderTexture(Target);
        return {};
    }
    return Target;
}

// Create a scene framebuffer with sampleable depth and an additional material-data attachment.
// Return the extra geometry texture through its separate owner so surface effects can read it
// without redrawing world geometry.
RenderTexture2D SurfaceTarget(int Width, int Height, Texture2D& CurrentGeometry) {
    RenderTexture2D Target{};
    Target.id = rlLoadFramebuffer();
    if (!Target.id)
        return {};
    constexpr int CurrentFormat = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    Target.texture = {rlLoadTexture(nullptr, Width, Height, CurrentFormat, 1), Width, Height, 1, CurrentFormat};
    Target.depth = {rlLoadTextureDepth(Width, Height, false), Width, Height, 1, 0};
    CurrentGeometry = {rlLoadTexture(nullptr, Width, Height, CurrentFormat, 1), Width, Height, 1, CurrentFormat};
    rlFramebufferAttach(Target.id, Target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(Target.id, CurrentGeometry.id, RL_ATTACHMENT_COLOR_CHANNEL1,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(Target.id, Target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D,
                        0);
    if (!CurrentGeometry.id || !Valid(Target)) {
        Release(Target);
        if (CurrentGeometry.id)
            UnloadTexture(CurrentGeometry);
        CurrentGeometry = {};
        return {};
    }
    SetTextureFilter(CurrentGeometry, TEXTURE_FILTER_POINT);
    SetTextureWrap(CurrentGeometry, TEXTURE_WRAP_CLAMP);
    return Target;
}
} // namespace

// Compile highlight-blur, composition, and surface-effect shaders and cache the uniforms required
// by successful custom programs. Leave unavailable shaders empty so the caller can use normal
// world rendering.
Bloom::Bloom() {
    CurrentShader = LoadShaderFromMemory(nullptr, MazeShaders::Bloom);
    CurrentComposite = LoadShaderFromMemory(nullptr, MazeShaders::Composite);
    SurfaceShader = LoadShaderFromMemory(nullptr, MazeShaders::SurfaceEffects);
    if (CurrentShader.id && CurrentShader.id != rlGetShaderIdDefault()) {
        StepLocation = GetShaderLocation(CurrentShader, "StepSize");
        ExtractLocation = GetShaderLocation(CurrentShader, "ExtractHighlights");
        HdrLocation = GetShaderLocation(CurrentShader, "LinearHdr");
    } else {
        CurrentShader = {};
    }
    if (CurrentComposite.id && CurrentComposite.id != rlGetShaderIdDefault()) {
        CompositeHdrLocation = GetShaderLocation(CurrentComposite, "LinearHdr");
        BloomLocation = GetShaderLocation(CurrentComposite, "BloomTexture");
        EffectsLocation = GetShaderLocation(CurrentComposite, "SurfaceTexture");
        EffectsEnabledLocation = GetShaderLocation(CurrentComposite, "SurfaceEffectsEnabled");
        CompositeDepthLocation = GetShaderLocation(CurrentComposite, "DepthTexture");
        CompositeInverseLocation = GetShaderLocation(CurrentComposite, "InverseProjection");
    } else {
        CurrentComposite = {};
    }
    if (SurfaceShader.id && SurfaceShader.id != rlGetShaderIdDefault()) {
        DepthLocation = GetShaderLocation(SurfaceShader, "DepthTexture");
        GeometryLocation = GetShaderLocation(SurfaceShader, "GeometryTexture");
        ViewLocation = GetShaderLocation(SurfaceShader, "ViewMatrix");
        PerspectiveLocation = GetShaderLocation(SurfaceShader, "ProjectionMatrix");
        InversePerspectiveLocation = GetShaderLocation(SurfaceShader, "InverseProjection");
    } else {
        SurfaceShader = {};
    }
}

// Release scene and intermediate render targets, the separately owned geometry attachment, and
// successfully loaded shaders. These GPU resources must be destroyed while the graphics context
// still exists.
Bloom::~Bloom() {
    Release(CurrentScene);
    Release(CurrentHorizontal);
    Release(Vertical);
    Release(CurrentSurfaceEffects);
    if (CurrentGeometry.id)
        UnloadTexture(CurrentGeometry);
    if (CurrentShader.id)
        UnloadShader(CurrentShader);
    if (CurrentComposite.id)
        UnloadShader(CurrentComposite);
    if (SurfaceShader.id)
        UnloadShader(SurfaceShader);
}

// Rebuild scene and intermediate targets when the requested dimensions change, reusing the
// current targets otherwise. Clean up incomplete allocations and remember attempted dimensions so
// failures are not retried every frame at the same size.
bool Bloom::Resize(int Width, int Height) {
    if (Width == AttemptedWidth && Height == AttemptedHeight)
        return CurrentScene.id != 0;
    AttemptedWidth = Width;
    AttemptedHeight = Height;
    Release(CurrentScene);
    Release(CurrentHorizontal);
    Release(Vertical);
    Release(CurrentSurfaceEffects);
    if (CurrentGeometry.id)
        UnloadTexture(CurrentGeometry);
    CurrentGeometry = {};
    if (SurfaceShader.id)
        CurrentScene = SurfaceTarget(Width, Height, CurrentGeometry);
    if (!CurrentScene.id)
        CurrentScene = HdrTarget(Width, Height);
    CurrentHorizontal = HdrTarget(std::max(1, Width / 2), std::max(1, Height / 2));
    Vertical = HdrTarget(std::max(1, Width / 2), std::max(1, Height / 2));
    if (CurrentGeometry.id) {
        const int EffectWidth = std::max(1, std::min(Width / 2, 720));
        const int EffectHeight = std::max(1, Height * EffectWidth / Width);
        CurrentSurfaceEffects = HdrTarget(EffectWidth, EffectHeight);
    }
    if (!Valid(CurrentScene) || !Valid(CurrentHorizontal) || !Valid(Vertical)) {
        Release(CurrentScene);
        Release(CurrentHorizontal);
        Release(Vertical);
        Release(CurrentSurfaceEffects);
        if (CurrentGeometry.id)
            UnloadTexture(CurrentGeometry);
        CurrentGeometry = {};
        return false;
    }
    for (Texture2D CurrentTexture : {CurrentScene.texture, CurrentHorizontal.texture, Vertical.texture}) {
        SetTextureFilter(CurrentTexture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(CurrentTexture, TEXTURE_WRAP_CLAMP);
    }
    if (CurrentSurfaceEffects.id) {
        SetTextureFilter(CurrentSurfaceEffects.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(CurrentSurfaceEffects.texture, TEXTURE_WRAP_CLAMP);
    }
    return true;
}

// Prepare targets for the current output size, save the caller's framebuffer and matrices, and
// begin scene capture. Return false when effects are unavailable so rendering can continue
// directly to the original destination.
bool Bloom::Begin(bool LinearHdr) {
    if (!CurrentShader.id || !CurrentComposite.id)
        return false;
    rlDrawRenderBatchActive();
    Destination = rlGetActiveFramebuffer();
    DestinationWidth = Destination ? GetScreenWidth() : GetRenderWidth();
    DestinationHeight = Destination ? GetScreenHeight() : GetRenderHeight();
    if (DestinationWidth <= 0 || DestinationHeight <= 0)
        return false;
    Projection = rlGetMatrixProjection();
    Modelview = rlGetMatrixModelview();
    if (!Resize(DestinationWidth, DestinationHeight)) {
        rlEnableFramebuffer(Destination);
        return false;
    }
    UseLinearHdr = LinearHdr;
    UseSurfaceEffects = false;
    BeginTextureMode(CurrentScene);
    if (CurrentGeometry.id)
        rlActiveDrawBuffers(2);
    return true;
}

// Capture the actual 3D view and projection matrices after BeginMode3D and enable surface effects
// only when their buffers and shader exist. Flush batches and disable blending while material
// data is written to attachments.
void Bloom::SetView(Matrix View, Matrix Perspective) {
    UseSurfaceEffects = UseLinearHdr && SurfaceShader.id && CurrentGeometry.id && CurrentSurfaceEffects.id;
    if (!UseSurfaceEffects)
        return;
    SurfaceView = View;
    SurfacePerspective = Perspective;
    InversePerspective = MatrixInvert(Perspective);
    // Reflectance is data in attachment alpha, not surface transparency.
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
}

// Finish scene capture, extract and blur highlights, optionally compute screen-space surface
// effects, and composite into the caller's saved framebuffer. Restore matrices and drawing state
// so later HUD and UI rendering remains unaffected.
void Bloom::End() {
    rlDrawRenderBatchActive();
    if (UseSurfaceEffects)
        rlEnableColorBlend();
    if (CurrentGeometry.id)
        rlActiveDrawBuffers(1);
    EndTextureMode();
    const int Hdr = UseLinearHdr ? 1 : 0;
    // Run one fullscreen blur pass into the supplied target with its source texture, sampling
    // direction, and extraction flag. Horizontal and vertical passes share this immediate
    // callback without reallocating shaders.
    auto Blur = [&](RenderTexture2D Target, Texture2D Source, Vector2 Step, int Extract) {
        BeginTextureMode(Target);
        ClearBackground(BLACK);
        BeginShaderMode(CurrentShader);
        SetShaderValue(CurrentShader, StepLocation, &Step, SHADER_UNIFORM_VEC2);
        SetShaderValue(CurrentShader, ExtractLocation, &Extract, SHADER_UNIFORM_INT);
        SetShaderValue(CurrentShader, HdrLocation, &Hdr, SHADER_UNIFORM_INT);
        DrawTarget(Source, Target.texture.width, Target.texture.height);
        EndShaderMode();
        EndTextureMode();
    };
    Blur(CurrentHorizontal, CurrentScene.texture, {3.0f / CurrentHorizontal.texture.width, 0}, 1);
    Blur(Vertical, CurrentHorizontal.texture, {0, 3.0f / Vertical.texture.height}, 0);
    if (UseSurfaceEffects) {
        BeginTextureMode(CurrentSurfaceEffects);
        ClearBackground(BLACK);
        rlDisableColorBlend();
        BeginShaderMode(SurfaceShader);
        SetShaderValueTexture(SurfaceShader, DepthLocation, CurrentScene.depth);
        SetShaderValueTexture(SurfaceShader, GeometryLocation, CurrentGeometry);
        SetShaderValueMatrix(SurfaceShader, ViewLocation, SurfaceView);
        SetShaderValueMatrix(SurfaceShader, PerspectiveLocation, SurfacePerspective);
        SetShaderValueMatrix(SurfaceShader, InversePerspectiveLocation, InversePerspective);
        DrawTarget(CurrentScene.texture, CurrentSurfaceEffects.texture.width, CurrentSurfaceEffects.texture.height);
        rlDrawRenderBatchActive();
        rlEnableColorBlend();
        EndShaderMode();
        EndTextureMode();
    }
    rlEnableFramebuffer(Destination);
    rlViewport(0, 0, DestinationWidth, DestinationHeight);
    rlSetMatrixProjection(Projection);
    rlSetMatrixModelview(Modelview);
    BeginShaderMode(CurrentComposite);
    SetShaderValue(CurrentComposite, CompositeHdrLocation, &Hdr, SHADER_UNIFORM_INT);
    SetShaderValueTexture(CurrentComposite, BloomLocation, Vertical.texture);
    const int Enabled = UseSurfaceEffects ? 1 : 0;
    SetShaderValue(CurrentComposite, EffectsEnabledLocation, &Enabled, SHADER_UNIFORM_INT);
    if (UseSurfaceEffects) {
        SetShaderValueTexture(CurrentComposite, EffectsLocation, CurrentSurfaceEffects.texture);
        SetShaderValueTexture(CurrentComposite, CompositeDepthLocation, CurrentScene.depth);
        SetShaderValueMatrix(CurrentComposite, CompositeInverseLocation, InversePerspective);
    }
    DrawTarget(CurrentScene.texture, GetScreenWidth(), GetScreenHeight());
    EndShaderMode();
}
