#include "MenuBackground.h"
#include "Ui.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {
// Recolor the red fabric while preserving the artwork's gold, marble and practical lighting.
constexpr const char* Grade = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec3 wallColor;
uniform float yaw;
uniform float aspect;
uniform int mirror;
out vec4 finalColor;
void main() {
    vec3 ray = vec3((fragTexCoord - 0.5) * vec2(2.0 * aspect, 2.0), 1.5);
    ray.xz = mat2(cos(yaw), -sin(yaw), sin(yaw), cos(yaw)) * ray.xz;
    vec2 uv = ray.xy / max(ray.z, 0.2) * vec2(0.75 / aspect, 0.75) + 0.5;
    if (mirror != 0) uv.x = 1.0 - uv.x;
    vec3 color = texture(texture0, uv).rgb;
    float value = max(max(color.r, color.g), color.b);
    float saturation = (value - min(min(color.r, color.g), color.b)) / max(value, 0.001);
    float redFabric = (1.0 - smoothstep(0.36, 0.60, color.g / max(color.r, 0.001)))
        * smoothstep(0.22, 0.60, saturation);
    float pigment = max(max(wallColor.r, wallColor.g), wallColor.b);
    vec3 dyed = wallColor / max(pigment, 0.04) * value * (0.58 + pigment * 0.50);
    // Dye the wall fabric while retaining the carpet's original red and gold pigments.
    float wallSurface = 1.0 - smoothstep(0.60, 0.82, uv.y);
    color = mix(color, dyed, redFabric * wallSurface * 0.90);
    vec2 vignette = (fragTexCoord - 0.5) * vec2(1.10, 0.85);
    color *= 1.0 - smoothstep(0.22, 0.64, length(vignette)) * 0.32;
    finalColor = vec4(color, 1.0) * fragColor;
}
)glsl";

std::string artworkPath(const char* filename) {
    const std::string packaged = std::string(GetApplicationDirectory()) + "assets/" + filename;
    if (FileExists(packaged.c_str()))
        return packaged;
    return std::string("src/assets/") + filename;
}

int viewIndex(MenuPlace place) {
    switch (place) {
    case MenuPlace::Players:
    case MenuPlace::Controls:
        return 1;
    case MenuPlace::Levels:
        return 2;
    case MenuPlace::Preparing:
        return 3;
    default:
        return 0;
    }
}
}

struct MenuBackground::Scene {
    std::array<Texture2D, 4> artwork{};
    Shader grade{};
    int wallLocation = -1;
    int yawLocation = -1, aspectLocation = -1, mirrorLocation = -1;
    MenuJourney journey;
    RenderTexture2D overlay{};
    bool suppressBackdrop = false;

    Scene() {
        const char* filenames[] = {"menu-corridor.png", "menu-players.png", "menu-levels.png",
                                   "menu-preparing.png"};
        for (int index = 0; index < (int)artwork.size(); ++index) {
            const std::string path = artworkPath(filenames[index]);
            if (FileExists(path.c_str())) {
                artwork[index] = LoadTexture(path.c_str());
                if (artwork[index].id) {
                    GenTextureMipmaps(&artwork[index]);
                    SetTextureFilter(artwork[index], TEXTURE_FILTER_TRILINEAR);
                    SetTextureWrap(artwork[index], TEXTURE_WRAP_CLAMP);
                }
            }
        }
        grade = LoadShaderFromMemory(nullptr, Grade);
        if (grade.id && grade.id != rlGetShaderIdDefault()) {
            wallLocation = GetShaderLocation(grade, "wallColor");
            yawLocation = GetShaderLocation(grade, "yaw");
            aspectLocation = GetShaderLocation(grade, "aspect");
            mirrorLocation = GetShaderLocation(grade, "mirror");
        } else
            grade = {};
    }

    ~Scene() {
        if (grade.id)
            UnloadShader(grade);
        if (overlay.id)
            UnloadRenderTexture(overlay);
        for (Texture2D texture : artwork)
            if (texture.id)
                UnloadTexture(texture);
    }

    void drawView(MenuPlace place, float zoom, float yaw, float alpha) const {
        const Texture2D image =
            artwork[viewIndex(place)].id ? artwork[viewIndex(place)] : artwork[0];
        if (!image.id || alpha <= 0)
            return;
        const int width = GetScreenWidth(), height = GetScreenHeight();
        const float cover = std::max(width / (float)image.width, height / (float)image.height);
        const float artWidth = std::max((float)width, image.width * cover * zoom);
        const float artHeight = std::max((float)height, image.height * cover * zoom);
        if (grade.id) {
            const Color pigment = mazePalette(MenuTheme::current()).wall;
            const float wall[] = {pigment.r / 255.0f, pigment.g / 255.0f, pigment.b / 255.0f};
            const float aspect = image.width / (float)image.height;
            const int mirror = place == MenuPlace::Controls ? 1 : 0;
            BeginShaderMode(grade);
            SetShaderValue(grade, wallLocation, wall, SHADER_UNIFORM_VEC3);
            SetShaderValue(grade, yawLocation, &yaw, SHADER_UNIFORM_FLOAT);
            SetShaderValue(grade, aspectLocation, &aspect, SHADER_UNIFORM_FLOAT);
            SetShaderValue(grade, mirrorLocation, &mirror, SHADER_UNIFORM_INT);
        }
        DrawTexturePro(image, {0, 0, (float)image.width, (float)image.height},
                       {(width - artWidth) * .5f, (height - artHeight) * .5f, artWidth, artHeight},
                       {0, 0}, 0, Fade(WHITE, alpha));
        if (grade.id)
            EndShaderMode();
    }
};

MenuBackground::MenuBackground() {
    static std::weak_ptr<Scene> shared;
    scene = shared.lock();
    if (!scene) {
        scene = std::make_shared<Scene>();
        shared = scene;
    }
}

// Each stop has a distinct connected view. Perspective yaw, dolly and phased overlap carry the
// camera around its corners; reversing a choice reverses the turn and travel direction.
void MenuBackground::draw(float time) const {
    if (scene->suppressBackdrop)
        return;
    const int width = GetScreenWidth(), height = GetScreenHeight();
    ClearBackground(MenuTheme::Ink);
    const MenuJourney& path = scene->journey;
    const float drift = .05f * (1 - std::exp(-std::max(0.0f, time) / 32.0f));
    const float base = 1.08f + drift + .06f * path.detailAmount();
    const float t = path.progress(), eased = MenuJourney::smooth(t);
    auto depth = [](MenuPlace place) { return viewIndex(place) * .025f; };
    if (!path.moving() || path.detailMove()) {
        scene->drawView(path.to(), base + depth(path.to()), path.detailAmount() * .025f, 1);
    } else {
        const float departing = path.backwards() ? -.05f : .22f;
        const float arriving = path.backwards() ? .20f : -.035f;
        scene->drawView(path.from(), base + depth(path.from()) + departing * eased,
                        path.turn() * .14f * eased, 1);
        if (path.to() != MenuPlace::Exit)
            scene->drawView(path.to(), base + depth(path.to()) + arriving * (1 - eased),
                            -path.turn() * .14f * (1 - eased),
                            MenuJourney::smooth((t - .30f) / .45f));
    }
    // Quiet the extremities for headings and navigation without washing out the architecture.
    DrawRectangleGradientV(0, 0, width, height / 3, Fade(MenuTheme::Ink, .30f), BLANK);
    DrawRectangleGradientV(0, height * 2 / 3, width, height - height * 2 / 3, BLANK,
                           Fade(MenuTheme::Ink, .64f));
    if (path.to() == MenuPlace::Exit)
        DrawRectangle(0, 0, width, height,
                      Fade(MenuTheme::Ink, MenuJourney::smooth((t - .45f) / .55f)));
}

bool MenuBackground::navigate(MenuPlace destination) {
    return scene->journey.goTo(destination);
}
void MenuBackground::detail(bool open) {
    scene->journey.showDetail(open);
}
void MenuBackground::snap(MenuPlace place) {
    scene->journey.snap(place);
}
void MenuBackground::update(float delta) {
    scene->journey.update(delta);
}
bool MenuBackground::moving() const {
    return scene->journey.moving();
}
bool MenuBackground::destinationVisible() const {
    return scene->journey.destinationVisible();
}

// Preserve the destination framebuffer and its matrices, including full-window preview targets.
// Capturing only UI lets the corridor keep moving with crisp, stable foreground typography.
void MenuBackground::drawOverlay(const std::function<void()>& drawUi) const {
    const float opacity = scene->journey.uiOpacity();
    if (opacity <= 0)
        return;
    rlDrawRenderBatchActive();
    const unsigned int destination = rlGetActiveFramebuffer();
    const int width = destination ? GetScreenWidth() : GetRenderWidth();
    const int height = destination ? GetScreenHeight() : GetRenderHeight();
    if (width <= 0 || height <= 0)
        return;
    const Matrix projection = rlGetMatrixProjection(), modelview = rlGetMatrixModelview();
    if (scene->overlay.texture.width != width || scene->overlay.texture.height != height) {
        if (scene->overlay.id)
            UnloadRenderTexture(scene->overlay);
        scene->overlay = LoadRenderTexture(width, height);
        if (scene->overlay.texture.id)
            SetTextureFilter(scene->overlay.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (!scene->overlay.id || !scene->overlay.texture.id) {
        rlEnableFramebuffer(destination);
        drawUi();
        return;
    }
    BeginTextureMode(scene->overlay);
    ClearBackground(BLANK);
    scene->suppressBackdrop = true;
    drawUi();
    scene->suppressBackdrop = false;
    EndTextureMode();
    rlEnableFramebuffer(destination);
    rlViewport(0, 0, width, height);
    rlSetMatrixProjection(projection);
    rlSetMatrixModelview(modelview);
    DrawTexturePro(scene->overlay.texture, {0, 0, (float)width, -(float)height},
                   {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, {0, 0}, 0,
                   Fade(WHITE, opacity));
}
