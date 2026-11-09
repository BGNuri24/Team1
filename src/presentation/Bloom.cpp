#include "Bloom.h"
#include "MazeShaders.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>

namespace {
// Require framebuffer, color, and depth handles and confirm framebuffer completeness before using
// a render target. Invalid targets cause post-processing to fall back rather than draw into an
// incomplete framebuffer.
bool valid(RenderTexture2D target) {
    return target.id && target.texture.id && target.depth.id && rlFramebufferComplete(target.id);
}

// Unload a render target when it owns a framebuffer, then clear every stored handle. Resetting
// the value lets resize and destruction share cleanup without reusing released GPU resources.
void release(RenderTexture2D& target) {
    if (target.id)
        UnloadRenderTexture(target);
    target = {};
}

// Draw a source texture across the requested pixel dimensions with an optional tint. Use a
// negative source height to correct render-texture orientation during fullscreen post-processing
// passes.
void drawTarget(Texture2D source, int width, int height, Color tint = WHITE) {
    DrawTexturePro(source, {0, 0, (float)source.width, -(float)source.height},
                   {0, 0, (float)width, (float)height}, {0, 0}, 0, tint);
}

// Create a render target whose color attachment stores floating-point RGBA16F light energy above
// one. Retain the normal depth-buffer ownership and release incomplete allocations before
// reporting failure.
RenderTexture2D hdrTarget(int width, int height) {
    RenderTexture2D target = LoadRenderTexture(width, height);
    if (!target.id)
        return {};
    const unsigned int color =
        rlLoadTexture(nullptr, width, height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    if (!color) {
        UnloadRenderTexture(target);
        return {};
    }
    const Texture2D previous = target.texture;
    target.texture = {color, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16};
    rlFramebufferAttach(target.id, color, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    UnloadTexture(previous);
    if (!valid(target)) {
        UnloadRenderTexture(target);
        return {};
    }
    return target;
}

// Create a scene framebuffer with sampleable depth and an additional material-data attachment.
// Return the extra geometry texture through its separate owner so surface effects can read it
// without redrawing world geometry.
RenderTexture2D surfaceTarget(int width, int height, Texture2D& geometry) {
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (!target.id)
        return {};
    constexpr int format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    target.texture = {rlLoadTexture(nullptr, width, height, format, 1), width, height, 1, format};
    target.depth = {rlLoadTextureDepth(width, height, false), width, height, 1, 0};
    geometry = {rlLoadTexture(nullptr, width, height, format, 1), width, height, 1, format};
    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target.id, geometry.id, RL_ATTACHMENT_COLOR_CHANNEL1,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D,
                        0);
    if (!geometry.id || !valid(target)) {
        release(target);
        if (geometry.id)
            UnloadTexture(geometry);
        geometry = {};
        return {};
    }
    SetTextureFilter(geometry, TEXTURE_FILTER_POINT);
    SetTextureWrap(geometry, TEXTURE_WRAP_CLAMP);
    return target;
}
} // namespace

// Compile highlight-blur, composition, and surface-effect shaders and cache the uniforms required
// by successful custom programs. Leave unavailable shaders empty so the caller can use normal
// world rendering.
Bloom::Bloom() {
    shader = LoadShaderFromMemory(nullptr, MazeShaders::Bloom);
    composite = LoadShaderFromMemory(nullptr, MazeShaders::Composite);
    surfaceShader = LoadShaderFromMemory(nullptr, MazeShaders::SurfaceEffects);
    if (shader.id && shader.id != rlGetShaderIdDefault()) {
        stepLocation = GetShaderLocation(shader, "stepSize");
        extractLocation = GetShaderLocation(shader, "extractHighlights");
        hdrLocation = GetShaderLocation(shader, "linearHdr");
    } else {
        shader = {};
    }
    if (composite.id && composite.id != rlGetShaderIdDefault()) {
        compositeHdrLocation = GetShaderLocation(composite, "linearHdr");
        bloomLocation = GetShaderLocation(composite, "bloomTexture");
        effectsLocation = GetShaderLocation(composite, "surfaceTexture");
        effectsEnabledLocation = GetShaderLocation(composite, "surfaceEffectsEnabled");
        compositeDepthLocation = GetShaderLocation(composite, "depthTexture");
        compositeInverseLocation = GetShaderLocation(composite, "inverseProjection");
    } else {
        composite = {};
    }
    if (surfaceShader.id && surfaceShader.id != rlGetShaderIdDefault()) {
        depthLocation = GetShaderLocation(surfaceShader, "depthTexture");
        geometryLocation = GetShaderLocation(surfaceShader, "geometryTexture");
        viewLocation = GetShaderLocation(surfaceShader, "viewMatrix");
        perspectiveLocation = GetShaderLocation(surfaceShader, "projectionMatrix");
        inversePerspectiveLocation = GetShaderLocation(surfaceShader, "inverseProjection");
    } else {
        surfaceShader = {};
    }
}

// Release scene and intermediate render targets, the separately owned geometry attachment, and
// successfully loaded shaders. These GPU resources must be destroyed while the graphics context
// still exists.
Bloom::~Bloom() {
    release(scene);
    release(horizontal);
    release(vertical);
    release(surfaceEffects);
    if (geometry.id)
        UnloadTexture(geometry);
    if (shader.id)
        UnloadShader(shader);
    if (composite.id)
        UnloadShader(composite);
    if (surfaceShader.id)
        UnloadShader(surfaceShader);
}

// Rebuild scene and intermediate targets when the requested dimensions change, reusing the
// current targets otherwise. Clean up incomplete allocations and remember attempted dimensions so
// failures are not retried every frame at the same size.
bool Bloom::resize(int width, int height) {
    if (width == attemptedWidth && height == attemptedHeight)
        return scene.id != 0;
    attemptedWidth = width;
    attemptedHeight = height;
    release(scene);
    release(horizontal);
    release(vertical);
    release(surfaceEffects);
    if (geometry.id)
        UnloadTexture(geometry);
    geometry = {};
    if (surfaceShader.id)
        scene = surfaceTarget(width, height, geometry);
    if (!scene.id)
        scene = hdrTarget(width, height);
    horizontal = hdrTarget(std::max(1, width / 2), std::max(1, height / 2));
    vertical = hdrTarget(std::max(1, width / 2), std::max(1, height / 2));
    if (geometry.id) {
        const int effectWidth = std::max(1, std::min(width / 2, 720));
        const int effectHeight = std::max(1, height * effectWidth / width);
        surfaceEffects = hdrTarget(effectWidth, effectHeight);
    }
    if (!valid(scene) || !valid(horizontal) || !valid(vertical)) {
        release(scene);
        release(horizontal);
        release(vertical);
        release(surfaceEffects);
        if (geometry.id)
            UnloadTexture(geometry);
        geometry = {};
        return false;
    }
    for (Texture2D texture : {scene.texture, horizontal.texture, vertical.texture}) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    }
    if (surfaceEffects.id) {
        SetTextureFilter(surfaceEffects.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(surfaceEffects.texture, TEXTURE_WRAP_CLAMP);
    }
    return true;
}

// Prepare targets for the current output size, save the caller's framebuffer and matrices, and
// begin scene capture. Return false when effects are unavailable so rendering can continue
// directly to the original destination.
bool Bloom::begin(bool linearHdr) {
    if (!shader.id || !composite.id)
        return false;
    rlDrawRenderBatchActive();
    destination = rlGetActiveFramebuffer();
    destinationWidth = destination ? GetScreenWidth() : GetRenderWidth();
    destinationHeight = destination ? GetScreenHeight() : GetRenderHeight();
    if (destinationWidth <= 0 || destinationHeight <= 0)
        return false;
    projection = rlGetMatrixProjection();
    modelview = rlGetMatrixModelview();
    if (!resize(destinationWidth, destinationHeight)) {
        rlEnableFramebuffer(destination);
        return false;
    }
    useLinearHdr = linearHdr;
    useSurfaceEffects = false;
    BeginTextureMode(scene);
    if (geometry.id)
        rlActiveDrawBuffers(2);
    return true;
}

// Capture the actual 3D view and projection matrices after BeginMode3D and enable surface effects
// only when their buffers and shader exist. Flush batches and disable blending while material
// data is written to attachments.
void Bloom::setView(Matrix view, Matrix perspective) {
    useSurfaceEffects = useLinearHdr && surfaceShader.id && geometry.id && surfaceEffects.id;
    if (!useSurfaceEffects)
        return;
    surfaceView = view;
    surfacePerspective = perspective;
    inversePerspective = MatrixInvert(perspective);
    // Reflectance is data in attachment alpha, not surface transparency.
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
}

// Finish scene capture, extract and blur highlights, optionally compute screen-space surface
// effects, and composite into the caller's saved framebuffer. Restore matrices and drawing state
// so later HUD and UI rendering remains unaffected.
void Bloom::end() {
    rlDrawRenderBatchActive();
    if (useSurfaceEffects)
        rlEnableColorBlend();
    if (geometry.id)
        rlActiveDrawBuffers(1);
    EndTextureMode();
    const int hdr = useLinearHdr ? 1 : 0;
    // Run one fullscreen blur pass into the supplied target with its source texture, sampling
    // direction, and extraction flag. Horizontal and vertical passes share this immediate
    // callback without reallocating shaders.
    auto blur = [&](RenderTexture2D target, Texture2D source, Vector2 step, int extract) {
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginShaderMode(shader);
        SetShaderValue(shader, stepLocation, &step, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, extractLocation, &extract, SHADER_UNIFORM_INT);
        SetShaderValue(shader, hdrLocation, &hdr, SHADER_UNIFORM_INT);
        drawTarget(source, target.texture.width, target.texture.height);
        EndShaderMode();
        EndTextureMode();
    };
    blur(horizontal, scene.texture, {3.0f / horizontal.texture.width, 0}, 1);
    blur(vertical, horizontal.texture, {0, 3.0f / vertical.texture.height}, 0);
    if (useSurfaceEffects) {
        BeginTextureMode(surfaceEffects);
        ClearBackground(BLACK);
        rlDisableColorBlend();
        BeginShaderMode(surfaceShader);
        SetShaderValueTexture(surfaceShader, depthLocation, scene.depth);
        SetShaderValueTexture(surfaceShader, geometryLocation, geometry);
        SetShaderValueMatrix(surfaceShader, viewLocation, surfaceView);
        SetShaderValueMatrix(surfaceShader, perspectiveLocation, surfacePerspective);
        SetShaderValueMatrix(surfaceShader, inversePerspectiveLocation, inversePerspective);
        drawTarget(scene.texture, surfaceEffects.texture.width, surfaceEffects.texture.height);
        rlDrawRenderBatchActive();
        rlEnableColorBlend();
        EndShaderMode();
        EndTextureMode();
    }
    rlEnableFramebuffer(destination);
    rlViewport(0, 0, destinationWidth, destinationHeight);
    rlSetMatrixProjection(projection);
    rlSetMatrixModelview(modelview);
    BeginShaderMode(composite);
    SetShaderValue(composite, compositeHdrLocation, &hdr, SHADER_UNIFORM_INT);
    SetShaderValueTexture(composite, bloomLocation, vertical.texture);
    const int enabled = useSurfaceEffects ? 1 : 0;
    SetShaderValue(composite, effectsEnabledLocation, &enabled, SHADER_UNIFORM_INT);
    if (useSurfaceEffects) {
        SetShaderValueTexture(composite, effectsLocation, surfaceEffects.texture);
        SetShaderValueTexture(composite, compositeDepthLocation, scene.depth);
        SetShaderValueMatrix(composite, compositeInverseLocation, inversePerspective);
    }
    drawTarget(scene.texture, GetScreenWidth(), GetScreenHeight());
    EndShaderMode();
}
