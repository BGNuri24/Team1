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
}

Bloom::Bloom() {
    shader = LoadShaderFromMemory(nullptr, MazeShaders::Bloom);
    if (shader.id && shader.id != rlGetShaderIdDefault()) {
        stepLocation = GetShaderLocation(shader, "stepSize");
        extractLocation = GetShaderLocation(shader, "extractHighlights");
    } else {
        shader = {};
    }
}

Bloom::~Bloom() {
    release(scene);
    release(horizontal);
    release(vertical);
    if (shader.id)
        UnloadShader(shader);
}

bool Bloom::resize(int width, int height) {
    if (width == attemptedWidth && height == attemptedHeight)
        return scene.id != 0;
    attemptedWidth = width;
    attemptedHeight = height;
    release(scene);
    release(horizontal);
    release(vertical);
    scene = LoadRenderTexture(width, height);
    horizontal = LoadRenderTexture(std::max(1, width / 2), std::max(1, height / 2));
    vertical = LoadRenderTexture(std::max(1, width / 2), std::max(1, height / 2));
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
bool Bloom::begin() {
    if (!shader.id)
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
    BeginTextureMode(scene);
    return true;
}

void Bloom::end() {
    EndTextureMode();
    auto blur = [&](RenderTexture2D target, Texture2D source, Vector2 step, int extract) {
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginShaderMode(shader);
        SetShaderValue(shader, stepLocation, &step, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, extractLocation, &extract, SHADER_UNIFORM_INT);
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
    drawTarget(scene.texture, GetScreenWidth(), GetScreenHeight());
    BeginBlendMode(BLEND_ADDITIVE);
    drawTarget(vertical.texture, GetScreenWidth(), GetScreenHeight(), {255, 255, 255, 175});
    EndBlendMode();
}
