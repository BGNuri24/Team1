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
uniform vec3 WallColor;
uniform float Yaw;
uniform float Aspect;
uniform float Clock;
uniform int Mirror;
out vec4 FinalColor;
// Warp corridor artwork for the current yaw and mirror state, tint wall fabric to the active
// palette, and add subtle lamp warmth and a vignette. Preserve the carpet pigments while applying
// the menu transition's vertex tint.
void main() {
    vec3 Ray = vec3((fragTexCoord - 0.5) * vec2(2.0 * Aspect, 2.0), 1.5);
    Ray.xz = mat2(cos(Yaw), -sin(Yaw), sin(Yaw), cos(Yaw)) * Ray.xz;
    vec2 Uv = Ray.xy / max(Ray.z, 0.2) * vec2(0.75 / Aspect, 0.75) + 0.5;
    if (Mirror != 0) Uv.x = 1.0 - Uv.x;
    vec3 Color = texture(texture0, Uv).rgb;
    float Value = max(max(Color.r, Color.g), Color.b);
    float Saturation = (Value - min(min(Color.r, Color.g), Color.b)) / max(Value, 0.001);
    float RedFabric = (1.0 - smoothstep(0.36, 0.60, Color.g / max(Color.r, 0.001)))
        * smoothstep(0.22, 0.60, Saturation);
    float Pigment = max(max(WallColor.r, WallColor.g), WallColor.b);
    vec3 Dyed = WallColor / max(Pigment, 0.04) * Value * (0.58 + Pigment * 0.50);
    // Dye the wall fabric while retaining the carpet's original red and gold pigments.
    float WallSurface = 1.0 - smoothstep(0.60, 0.82, Uv.y);
    Color = mix(Color, Dyed, RedFabric * WallSurface * 0.90);
    // Slow, low-amplitude warmth in the brightest lamp pixels, without flashing the whole view.
    float Lamp = smoothstep(0.64, 0.96, dot(Color, vec3(0.2126, 0.7152, 0.0722)))
        * (1.0 - smoothstep(0.48, 0.78, Saturation));
    float Breath = sin(Clock * 1.15 + Uv.x * 3.0 + Uv.y * 4.0) * 0.75
        + sin(Clock * 0.31 - Uv.y * 5.0) * 0.25;
    Color += Color * vec3(1.0, 0.88, 0.65) * Lamp * Breath * 0.080;
    vec2 Vignette = (fragTexCoord - 0.5) * vec2(1.10, 0.85);
    Color *= 1.0 - smoothstep(0.22, 0.64, length(Vignette)) * 0.32;
    FinalColor = vec4(Color, 1.0) * fragColor;
}
)glsl";

// Resolve menu artwork beside the executable first, falling back to src/assets for development
// runs. Return a path only; the caller checks availability and owns any subsequently loaded
// texture.
std::string ArtworkPath(const char* Filename) {
    const std::string Packaged = std::string(GetApplicationDirectory()) + "assets/" + Filename;
    if (FileExists(Packaged.c_str()))
        return Packaged;
    return std::string("src/assets/") + Filename;
}

// Map menu stops to the four shared corridor artwork slots. Controls reuses the player view with
// mirroring, while entrance and fallback stops use the first image.
int ViewIndex(MenuPlace CurrentPlace) {
    switch (CurrentPlace) {
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
} // namespace

struct MenuBackground::Scene {
    std::array<Texture2D, 4> Artwork{};
    Shader CurrentGrade{};
    int WallLocation = -1;
    int YawLocation = -1, AspectLocation = -1, MirrorLocation = -1;
    int ClockLocation = -1;
    MenuJourney Journey;
    RenderTexture2D Overlay{};
    bool SuppressBackdrop = false;

    // Load corridor artwork with mipmaps and compile the grading shader, caching uniform
    // locations only for a valid custom shader. The shared scene owns these resources and the
    // journey state used by every menu.
    Scene() {
        const char* Filenames[] = {"MenuCorridor.png", "MenuPlayers.png", "MenuLevels.png",
                                   "MenuPreparing.png"};
        for (int Index = 0; Index < (int)Artwork.size(); ++Index) {
            const std::string Path = ArtworkPath(Filenames[Index]);
            if (FileExists(Path.c_str())) {
                Artwork[Index] = LoadTexture(Path.c_str());
                if (Artwork[Index].id) {
                    GenTextureMipmaps(&Artwork[Index]);
                    SetTextureFilter(Artwork[Index], TEXTURE_FILTER_TRILINEAR);
                    SetTextureWrap(Artwork[Index], TEXTURE_WRAP_CLAMP);
                }
            }
        }
        CurrentGrade = LoadShaderFromMemory(nullptr, Grade);
        if (CurrentGrade.id && CurrentGrade.id != rlGetShaderIdDefault()) {
            WallLocation = GetShaderLocation(CurrentGrade, "WallColor");
            YawLocation = GetShaderLocation(CurrentGrade, "Yaw");
            AspectLocation = GetShaderLocation(CurrentGrade, "Aspect");
            MirrorLocation = GetShaderLocation(CurrentGrade, "Mirror");
            ClockLocation = GetShaderLocation(CurrentGrade, "Clock");
        } else
            CurrentGrade = {};
    }

    // Release the shared grading shader, overlay render target, and all successfully loaded
    // artwork textures. Destruction occurs when the last MenuBackground owner releases the scene,
    // before the graphics context closes.
    ~Scene() {
        if (CurrentGrade.id)
            UnloadShader(CurrentGrade);
        if (Overlay.id)
            UnloadRenderTexture(Overlay);
        for (Texture2D CurrentTexture : Artwork)
            if (CurrentTexture.id)
                UnloadTexture(CurrentTexture);
    }

    // Draw the selected corridor image with cover sizing, zoom, yaw, and gentle time-based
    // panning. Fall back to the entrance image when needed, skip missing or transparent views,
    // and apply the grading shader when available.
    void DrawView(MenuPlace CurrentPlace, float Zoom, float Yaw, float Alpha, float CurrentTime) const {
        const Texture2D CurrentImage =
            Artwork[ViewIndex(CurrentPlace)].id ? Artwork[ViewIndex(CurrentPlace)] : Artwork[0];
        if (!CurrentImage.id || Alpha <= 0)
            return;
        const int Width = GetScreenWidth(), Height = GetScreenHeight();
        const float CurrentCover = std::max(Width / (float)CurrentImage.width, Height / (float)CurrentImage.height);
        const float ArtWidth = std::max((float)Width, CurrentImage.width * CurrentCover * Zoom);
        const float ArtHeight = std::max((float)Height, CurrentImage.height * CurrentCover * Zoom);
        const float Phase = ViewIndex(CurrentPlace) * 1.19f;
        const float PanX = Width * .014f * std::sin(CurrentTime * .35f + Phase);
        const float PanY = Height * .006f * std::sin(CurrentTime * .27f + Phase * .6f);
        if (CurrentGrade.id) {
            const Color Pigment = GetMazePalette(MenuTheme::Current()).CurrentWall;
            const float CurrentWall[] = {Pigment.r / 255.0f, Pigment.g / 255.0f, Pigment.b / 255.0f};
            const float Aspect = CurrentImage.width / (float)CurrentImage.height;
            const int Mirror = CurrentPlace == MenuPlace::Controls ? 1 : 0;
            BeginShaderMode(CurrentGrade);
            SetShaderValue(CurrentGrade, WallLocation, CurrentWall, SHADER_UNIFORM_VEC3);
            SetShaderValue(CurrentGrade, YawLocation, &Yaw, SHADER_UNIFORM_FLOAT);
            SetShaderValue(CurrentGrade, AspectLocation, &Aspect, SHADER_UNIFORM_FLOAT);
            SetShaderValue(CurrentGrade, MirrorLocation, &Mirror, SHADER_UNIFORM_INT);
            SetShaderValue(CurrentGrade, ClockLocation, &CurrentTime, SHADER_UNIFORM_FLOAT);
        }
        DrawTexturePro(CurrentImage, {0, 0, (float)CurrentImage.width, (float)CurrentImage.height},
                       {(Width - ArtWidth) * .5f + PanX, (Height - ArtHeight) * .5f + PanY,
                        ArtWidth, ArtHeight},
                       {0, 0}, 0, Fade(WHITE, Alpha));
        if (CurrentGrade.id)
            EndShaderMode();
    }
};

// Reuse the existing scene through a weak shared cache, creating it only when no menu currently
// owns one. Each backdrop keeps a shared_ptr so textures and journey state survive until the
// final owner is destroyed.
MenuBackground::MenuBackground() {
    static std::weak_ptr<Scene> CurrentShared;
    CurrentScene = CurrentShared.lock();
    if (!CurrentScene) {
        CurrentScene = std::make_shared<Scene>();
        CurrentShared = CurrentScene;
    }
}

// Render the shared corridor at its current journey position, blending source and destination
// artwork and applying the route's zoom and turn. Drawing reads animation state; update is
// responsible for advancing time.
void MenuBackground::Draw(float CurrentTime) const {
    if (CurrentScene->SuppressBackdrop)
        return;
    const int Width = GetScreenWidth(), Height = GetScreenHeight();
    ClearBackground(MenuTheme::Ink);
    const MenuJourney& Path = CurrentScene->Journey;
    const float Clock = std::isfinite(CurrentTime) ? std::max(0.0f, CurrentTime) : 0.0f;
    // Bounded idle movement continues for as long as the menu is open, with overscan at each edge.
    const float Base = 1.12f + .014f * std::sin(Clock * .22f) + .06f * Path.DetailAmount();
    const float IdleYaw = -.010f * std::sin(Clock * .23f);
    const float CurrentT = Path.Progress(), Eased = MenuJourney::Smooth(CurrentT);
    // Derive a small depth offset from the corridor artwork index for route zooming. This
    // deterministic presentation value does not change the journey's destination or timing.
    auto Depth = [](MenuPlace CurrentPlace) { return ViewIndex(CurrentPlace) * .025f; };
    if (!Path.Moving() || Path.DetailMove()) {
        CurrentScene->DrawView(Path.To(), Base + Depth(Path.To()), Path.DetailAmount() * .025f + IdleYaw,
                        1, Clock);
    } else {
        const float Departing = Path.Backwards() ? -.05f : .22f;
        const float Arriving = Path.Backwards() ? .20f : -.035f;
        CurrentScene->DrawView(Path.From(), Base + Depth(Path.From()) + Departing * Eased,
                        Path.Turn() * .14f * Eased + IdleYaw, 1, Clock);
        if (Path.To() != MenuPlace::Exit)
            CurrentScene->DrawView(Path.To(), Base + Depth(Path.To()) + Arriving * (1 - Eased),
                            -Path.Turn() * .14f * (1 - Eased) + IdleYaw,
                            MenuJourney::Smooth((CurrentT - .30f) / .45f), Clock);
    }
    // Quiet the extremities for headings and navigation without washing out the architecture.
    DrawRectangleGradientV(0, 0, Width, Height / 3, Fade(MenuTheme::Ink, .30f), BLANK);
    DrawRectangleGradientV(0, Height * 2 / 3, Width, Height - Height * 2 / 3, BLANK,
                           Fade(MenuTheme::Ink, .64f));
    if (Path.To() == MenuPlace::Exit)
        DrawRectangle(0, 0, Width, Height,
                      Fade(MenuTheme::Ink, MenuJourney::Smooth((CurrentT - .45f) / .55f)));
}

// Request a route to a destination through the shared MenuJourney. Return whether the trip was
// accepted so Application can defer publishing its pending screen until travel finishes.
bool MenuBackground::Navigate(MenuPlace Destination) {
    return CurrentScene->Journey.GoTo(Destination);
}
// Request an opening or closing detail animation on the shared journey. Existing active travel
// and redundant requests are ignored by MenuJourney to keep navigation deterministic.
void MenuBackground::Detail(bool CurrentOpen) {
    CurrentScene->Journey.ShowDetail(CurrentOpen);
}
// Immediately select a corridor stop through the shared journey, clearing its detail and travel
// state. Use this when returning from a run so the backdrop agrees with the visible menu.
void MenuBackground::Snap(MenuPlace CurrentPlace) {
    CurrentScene->Journey.Snap(CurrentPlace);
}
// Advance the shared journey using a frame duration in seconds. Invalid or nonpositive durations
// are ignored by MenuJourney, and screen code controls when animation time is allowed to pass.
void MenuBackground::Update(float Delta) {
    CurrentScene->Journey.Update(Delta);
}
// Expose whether the shared journey is active without modifying it. Application uses this to keep
// page input frozen during route or detail animations.
bool MenuBackground::Moving() const {
    return CurrentScene->Journey.Moving();
}
// Expose the journey's midpoint decision for choosing which foreground menu to draw. This does
// not finish navigation or advance animation timing.
bool MenuBackground::DestinationVisible() const {
    return CurrentScene->Journey.DestinationVisible();
}

// Capture foreground UI into a reusable render target and composite it with the journey's
// opacity. Keep corridor artwork outside that capture so its turn and grade continue
// independently behind text.
void MenuBackground::DrawOverlay(const std::function<void()>& DrawUi) const {
    const float Opacity = CurrentScene->Journey.UiOpacity();
    if (Opacity <= 0)
        return;
    rlDrawRenderBatchActive();
    const unsigned int Destination = rlGetActiveFramebuffer();
    const int Width = Destination ? GetScreenWidth() : GetRenderWidth();
    const int Height = Destination ? GetScreenHeight() : GetRenderHeight();
    if (Width <= 0 || Height <= 0)
        return;
    const Matrix Projection = rlGetMatrixProjection(), Modelview = rlGetMatrixModelview();
    if (CurrentScene->Overlay.texture.width != Width || CurrentScene->Overlay.texture.height != Height) {
        if (CurrentScene->Overlay.id)
            UnloadRenderTexture(CurrentScene->Overlay);
        CurrentScene->Overlay = LoadRenderTexture(Width, Height);
        if (CurrentScene->Overlay.texture.id)
            SetTextureFilter(CurrentScene->Overlay.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (!CurrentScene->Overlay.id || !CurrentScene->Overlay.texture.id) {
        rlEnableFramebuffer(Destination);
        DrawUi();
        return;
    }
    BeginTextureMode(CurrentScene->Overlay);
    ClearBackground(BLANK);
    CurrentScene->SuppressBackdrop = true;
    DrawUi();
    CurrentScene->SuppressBackdrop = false;
    EndTextureMode();
    rlEnableFramebuffer(Destination);
    rlViewport(0, 0, Width, Height);
    rlSetMatrixProjection(Projection);
    rlSetMatrixModelview(Modelview);
    DrawTexturePro(CurrentScene->Overlay.texture, {0, 0, (float)Width, -(float)Height},
                   {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, {0, 0}, 0,
                   Fade(WHITE, Opacity));
}
