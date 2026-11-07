#include "Bloom.h"
#include "MazeShaders.h"
#include "rlgl.h"
#include <algorithm>

namespace {
bool valid(RenderTexture2D target) {
    return target.id && target.texture.id && target.depth.id && rlFramebufferComplete(target.id);
}

void release(RenderTexture2D& target) {
    if (target.id)
        UnloadRenderTexture(target);
    target = {};
}

void drawTarget(Texture2D source, int width, int height, Color tint = WHITE) {
    DrawTexturePro(source, {0, 0, (float)source.width, -(float)source.height},
                   {0, 0, (float)width, (float)height}, {0, 0}, 0, tint);
}

// Keep light energy above one through bloom. Replace raylib's 8-bit color attachment with a
// filterable RGBA16F texture, retaining its normal depth-buffer owner and cleanup path.
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
}

Bloom::Bloom() {
    shader = LoadShaderFromMemory(nullptr, MazeShaders::Bloom);
    composite = LoadShaderFromMemory(nullptr, MazeShaders::Composite);
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
    } else {
        composite = {};
    }
}

Bloom::~Bloom() {
    release(scene);
    release(horizontal);
    release(vertical);
    if (shader.id)
        UnloadShader(shader);
    if (composite.id)
        UnloadShader(composite);
}

bool Bloom::resize(int width, int height) {
    if (width == attemptedWidth && height == attemptedHeight)
        return scene.id != 0;
    attemptedWidth = width;
    attemptedHeight = height;
    release(scene);
    release(horizontal);
    release(vertical);
    scene = hdrTarget(width, height);
    horizontal = hdrTarget(std::max(1, width / 2), std::max(1, height / 2));
    vertical = hdrTarget(std::max(1, width / 2), std::max(1, height / 2));
    if (!valid(scene) || !valid(horizontal) || !valid(vertical)) {
        release(scene);
        release(horizontal);
        release(vertical);
        return false;
    }
    for (Texture2D texture : {scene.texture, horizontal.texture, vertical.texture}) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    }
    return true;
}

// Preserve the caller's framebuffer as well as its matrices, including full-window screenshot
// targets. Drawing the world does not depend on the screen being the active framebuffer.
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
    BeginTextureMode(scene);
    return true;
}

void Bloom::end() {
    EndTextureMode();
    const int hdr = useLinearHdr ? 1 : 0;
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
    rlEnableFramebuffer(destination);
    rlViewport(0, 0, destinationWidth, destinationHeight);
    rlSetMatrixProjection(projection);
    rlSetMatrixModelview(modelview);
    BeginShaderMode(composite);
    SetShaderValue(composite, compositeHdrLocation, &hdr, SHADER_UNIFORM_INT);
    SetShaderValueTexture(composite, bloomLocation, vertical.texture);
    drawTarget(scene.texture, GetScreenWidth(), GetScreenHeight());
    EndShaderMode();
}
