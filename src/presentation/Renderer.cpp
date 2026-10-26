#include "Renderer.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float VIEW_DISTANCE = 15.0f;
constexpr Color FOG = {7, 16, 21, 255};
constexpr Color AMBER = {244, 190, 102, 255};
constexpr Color MINT = {113, 216, 182, 255};

// Per-pixel fog and a broad, soft light that follows the player's view.
// All shader source is embedded: a fresh checkout needs no asset downloads.
const char* VERTEX_SHADER = R"glsl(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 worldPosition;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    worldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

const char* FRAGMENT_SHADER = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 worldPosition;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 eyePosition;
uniform vec3 viewDirection;
uniform float emission;
out vec4 finalColor;
void main() {
    vec4 surface = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 offset = worldPosition - eyePosition;
    float distance = length(offset);
    float cone = smoothstep(0.48, 0.98, dot(offset / max(distance, 0.001), viewDirection));
    float nearby = exp(-distance * 0.23);
    vec3 light = vec3(0.37, 0.52, 0.59) + nearby * vec3(0.42, 0.43, 0.37)
        + cone * exp(-distance * 0.12) * vec3(0.75, 0.62, 0.40);
    vec3 lit = surface.rgb * mix(light, vec3(1.0), emission);
    float fog = smoothstep(3.0, 15.0, distance);
    finalColor = vec4(mix(lit, vec3(0.0275, 0.0627, 0.0824), fog), surface.a);
}
)glsl";

struct Corner { float x, y, z, u, v; };
struct Wall { float lx, lz, rx, rz; int row, col, side; };

void emit(const Corner& point, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlTexCoord2f(point.u, point.v);
    rlVertex3f(point.x, point.y, point.z);
}

void quad(const Corner& a, const Corner& b, const Corner& c, const Corner& d, Color color) {
    emit(a, color); emit(b, color); emit(c, color); emit(d, color);
}

// Local coordinates on an exposed wall. The offset keeps trim above the material.
void wallStrip(const Wall& wall, float left, float right, float bottom, float top,
               Color color, float inset = 0.004f) {
    float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
    float x0 = wall.lx + dx * left - dz * inset;
    float z0 = wall.lz + dz * left + dx * inset;
    float x1 = wall.lx + dx * right - dz * inset;
    float z1 = wall.lz + dz * right + dx * inset;
    quad({x0, bottom, z0, left, 1-bottom}, {x1, bottom, z1, right, 1-bottom},
         {x1, top, z1, right, 1-top}, {x0, top, z0, left, 1-top}, color);
}

template <typename Draw> void nearbyCells(const Game& game, Draw draw) {
    int reach = (int)VIEW_DISTANCE + 1;
    int size = game.maze().size();
    for (int row = std::max(0, game.row()-reach); row <= std::min(size-1, game.row()+reach); ++row)
        for (int col = std::max(0, game.col()-reach); col <= std::min(size-1, game.col()+reach); ++col) {
            if (game.maze().cell(row, col) == '#') continue;
            float dx = col - game.x(), dz = row - game.z();
            if (dx*dx + dz*dz > (VIEW_DISTANCE+1)*(VIEW_DISTANCE+1)) continue;
            draw(row, col);
        }
}

template <typename Draw> void exposedWalls(const Game& game, Draw draw) {
    nearbyCells(game, [&](int row, int col) {
        float x = (float)col, z = (float)row;
        if (game.maze().cell(row-1, col) == '#') draw(Wall{x-.5f, z-.5f, x+.5f, z-.5f, row, col, 0});
        if (game.maze().cell(row+1, col) == '#') draw(Wall{x+.5f, z+.5f, x-.5f, z+.5f, row, col, 1});
        if (game.maze().cell(row, col-1) == '#') draw(Wall{x-.5f, z+.5f, x-.5f, z-.5f, row, col, 2});
        if (game.maze().cell(row, col+1) == '#') draw(Wall{x+.5f, z-.5f, x+.5f, z+.5f, row, col, 3});
    });
}

void floorPatch(float x, float z, float y, float halfWidth, float halfDepth, Color color) {
    quad({x-halfWidth,y,z-halfDepth,0,0}, {x-halfWidth,y,z+halfDepth,0,1},
         {x+halfWidth,y,z+halfDepth,1,1}, {x+halfWidth,y,z-halfDepth,1,0}, color);
}

float wrapAngle(float angle) {
    return std::atan2(std::sin(angle), std::cos(angle));
}

void hudPanel(Rectangle bounds) {
    DrawRectangleRounded(bounds, 0.10f, 8, {9, 22, 29, 225});
    DrawRectangleRoundedLinesEx(bounds, 0.10f, 8, std::max(1.0f, uiScale()), {55, 80, 83, 175});
}
}

Renderer::Renderer() {
    cam = Camera3D{};
    cam.up = {0, 1, 0};
    cam.fovy = 70;
    cam.projection = CAMERA_PERSPECTIVE;
    tex = createTextures();
    atmosphere = LoadShaderFromMemory(VERTEX_SHADER, FRAGMENT_SHADER);
    if (atmosphere.id != 0 && atmosphere.id != rlGetShaderIdDefault()) {
        eyeLocation = GetShaderLocation(atmosphere, "eyePosition");
        directionLocation = GetShaderLocation(atmosphere, "viewDirection");
        emissionLocation = GetShaderLocation(atmosphere, "emission");
    } else {
        atmosphere = {};
    }
}

Renderer::~Renderer() {
    destroyTextures(tex);
    if (atmosphere.id) UnloadShader(atmosphere);
}

void Renderer::draw(const Game& game) {
    cam.position = game.position();
    const Vector3 direction = game.cameraDirection();
    cam.target = {cam.position.x + direction.x, cam.position.y + direction.y, cam.position.z + direction.z};
    cam.up = game.cameraUp();

    ClearBackground(FOG);
    BeginMode3D(cam);
    if (atmosphere.id) {
        float emission = 0;
        SetShaderValue(atmosphere, eyeLocation, &cam.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(atmosphere, directionLocation, &direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(atmosphere, emissionLocation, &emission, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(atmosphere);
    }
    drawMaze(game);
    // Flush before changing a uniform, so the surfaces keep their own lighting.
    rlDrawRenderBatchActive();
    if (atmosphere.id) {
        float emission = 1;
        SetShaderValue(atmosphere, emissionLocation, &emission, SHADER_UNIFORM_FLOAT);
    }
    drawLights(game);
    if (atmosphere.id) EndShaderMode();
    EndMode3D();

    const int width = GetScreenWidth(), height = GetScreenHeight();
    DrawRectangleGradientV(0, 0, width, uiSize(160), {3, 10, 15, 115}, {3, 10, 15, 0});
    DrawRectangleGradientV(0, height-uiSize(130), width, uiSize(130), {3, 10, 15, 0}, {3, 10, 15, 160});
    drawHud(game);
    drawMinimap(game);
}

void Renderer::drawMaze(const Game& game) {
    rlDisableBackfaceCulling();
    rlSetTexture(tex.wall.id);
    rlBegin(RL_QUADS);
    exposedWalls(game, [](const Wall& wall) {
        unsigned char brightness = wall.side < 2 ? 255 : 222;
        wallStrip(wall, 0, 1, 0, 1, {brightness, brightness, brightness, 255}, 0);
    });
    rlEnd();

    rlSetTexture(tex.floor.id);
    rlBegin(RL_QUADS);
    nearbyCells(game, [](int row, int col) {
        floorPatch((float)col, (float)row, 0, .5f, .5f, {235, 238, 235, 255});
    });
    rlEnd();

    rlSetTexture(tex.ceiling.id);
    rlBegin(RL_QUADS);
    nearbyCells(game, [](int row, int col) {
        floorPatch((float)col, (float)row, 1, .5f, .5f, {190, 207, 216, 255});
    });
    rlEnd();

    rlSetTexture(0);
    rlBegin(RL_QUADS);
    exposedWalls(game, [](const Wall& wall) {
        // Flush ribs and skirting; every piece stays against the collision surface.
        wallStrip(wall, .015f, .043f, .03f, .97f, {48, 78, 82, 255});
        wallStrip(wall, .957f, .985f, .03f, .97f, {32, 54, 61, 255});
        wallStrip(wall, 0, 1, .025f, .065f, {21, 39, 45, 255});
        wallStrip(wall, 0, 1, .935f, .975f, {50, 75, 78, 255});
    });
    rlEnd();
    rlSetTexture(0);
    // Culling is global OpenGL state, not stored with queued batch vertices.
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

void Renderer::drawLights(const Game& game) {
    const Color accent = game.mode() == GameMode::ZeroGravity ? MINT : AMBER;
    const float time = (float)GetTime();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_QUADS);
    exposedWalls(game, [&](const Wall& wall) {
        wallStrip(wall, .09f, .91f, .097f, .103f, Fade(accent, .65f), .006f);
        const int pattern = (wall.row*13 + wall.col*7 + wall.side*3) % 5;
        if (pattern > 1) return;
        const Color lamp = pattern == 0 ? accent : Color{155, 215, 218, 255};
        wallStrip(wall, .31f, .69f, .785f, .865f, {20, 38, 44, 255}, .006f);
        wallStrip(wall, .33f, .67f, .795f, .855f, Fade(lamp, .10f), .008f);
        wallStrip(wall, .35f, .65f, .817f, .833f, lamp, .010f);
        wallStrip(wall, .34f, .36f, .795f, .855f, {93, 114, 109, 255}, .012f);
        wallStrip(wall, .64f, .66f, .795f, .855f, {93, 114, 109, 255}, .012f);
    });
    nearbyCells(game, [&](int row, int col) {
        float x = (float)col, z = (float)row;
        if ((row + col) % 3 == 0) {
            floorPatch(x, z, .982f, .24f, .07f, {25, 42, 47, 255});
            floorPatch(x, z, .979f, .18f, .022f, {161, 194, 187, 255});
        }
        if (game.seenState(row, col) == 2) {
            // Breadcrumbs mark only places actually walked, never the solution.
            for (int i = -1; i <= 1; ++i)
                floorPatch(x + i*.055f, z, .004f, .009f, .009f, Fade(accent, .62f));
        }
    });
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();

    // A recognizable doorway replaces the old solid green cube.
    const int last = game.maze().size()-1;
    float dx = last-game.x(), dz = last-game.z();
    if (dx*dx + dz*dz > VIEW_DISTANCE*VIEW_DISTANCE) return;
    bool northEntrance = game.maze().cell(last-1, last) != '#';
    auto point = [last, northEntrance](float across, float y) -> Vector3 {
        return northEntrance ? Vector3{last+across, y, last+.31f}
                             : Vector3{last+.31f, y, last+across};
    };
    float pulse = .85f + .15f * std::sin(time*1.7f);
    for (float side : {-.34f, .34f}) {
        DrawCube(point(side, .47f), .075f, .88f, .075f, {35, 76, 73, 255});
        Vector3 light = point(side, .47f);
        if (northEntrance) light.z -= .041f;
        else light.x -= .041f;
        DrawCube(light, northEntrance ? .024f : .006f, .80f,
                 northEntrance ? .006f : .024f, MINT);
    }
    DrawCube(point(0, .9f), northEntrance ? .75f : .08f, .07f, northEntrance ? .08f : .75f, MINT);
    DrawCube(point(0, .06f), northEntrance ? .75f : .08f, .045f, northEntrance ? .08f : .75f, MINT);
    DrawCube({(float)last, .012f, (float)last}, .72f, .018f, .72f, {31, 83, 71, 255});
    DrawCubeWires({(float)last, .025f, (float)last}, .73f, .02f, .73f, MINT);
    for (int i = 0; i < 4; ++i) {
        float y = .22f + std::fmod(time*.12f + i*.18f, .55f);
        DrawCube(point(0, y), northEntrance ? .54f : .008f, .007f,
                 northEntrance ? .008f : .54f, Fade(MINT, .12f*pulse));
    }
}

void Renderer::drawHud(const Game& game) {
    using namespace MenuTheme;
    const int margin = uiSize(22);
    auto text = [&](const char* label, int x, int y, int size, Color color) {
        DrawText(label, x, y, std::max(1, uiSize(size)), color);
    };
    const int remaining = std::max(0, (int)std::ceil(game.timeLeft()));
    const bool urgent = remaining <= 30;
    const Color clockColor = urgent ? Danger : Paper;
    hudPanel({(float)margin, (float)margin, (float)uiSize(220), (float)uiSize(118)});
    DrawRectangle(margin, margin+uiSize(20), std::max(2, uiSize(3)), uiSize(28), urgent ? Danger : Amber);
    text("TIME TO FIND A WAY", margin+uiSize(18), margin+uiSize(14), 12, Muted);
    text(TextFormat("%02d:%02d", remaining/60, remaining%60), margin+uiSize(16), margin+uiSize(35), 42, clockColor);
    const bool zero = game.mode() == GameMode::ZeroGravity;
    text(zero ? "ZERO GRAVITY" : "THE LABYRINTH", margin+uiSize(18), margin+uiSize(88), 13, zero ? Mint : Amber);
    float duration = game.timeLeft()+game.timeTaken();
    float ratio = duration > 0 ? std::clamp(game.timeLeft()/duration, 0.0f, 1.0f) : 0;
    DrawRectangle(margin+uiSize(18), margin+uiSize(108), uiSize(184), std::max(1, uiSize(2)), Border);
    DrawRectangle(margin+uiSize(18), margin+uiSize(108), (int)(uiSize(184)*ratio), std::max(1, uiSize(2)), urgent ? Danger : Amber);

    // Heading tape: the cardinal directions refer to the actual maze axes.
    const int center = GetScreenWidth()/2;
    const int compassWidth = uiSize(220);
    const int compassY = margin+uiSize(12);
    DrawLine(center-compassWidth/2, compassY+uiSize(24), center+compassWidth/2, compassY+uiSize(24), Fade(Border, .8f));
    for (int degree = 0; degree < 360; degree += 15) {
        float angle = degree*DEG2RAD;
        float offset = wrapAngle(angle-game.yaw());
        if (std::fabs(offset) > 1.0f) continue;
        int x = center+(int)(offset*compassWidth*.5f);
        DrawLine(x, compassY+uiSize(20), x, compassY+uiSize(24), Muted);
        if (degree%90 == 0) {
            const char* direction = degree == 0 ? "E" : degree == 90 ? "S" : degree == 180 ? "W" : "N";
            DrawText(direction, x-MeasureText(direction,uiSize(14))/2, compassY, uiSize(14), Paper);
        }
    }
    DrawTriangle({(float)center, (float)(compassY+uiSize(29))},
                 {(float)(center-uiSize(3)), (float)(compassY+uiSize(35))},
                 {(float)(center+uiSize(3)), (float)(compassY+uiSize(35))}, Amber);

    const int footer = GetScreenHeight()-uiSize(40);
    text("ESC  pause", margin, footer, 15, Muted);
    if (zero)
        text(game.surfaceAttached() ? "G  return to floor" : "G  attach to a wall or ceiling", margin, footer-uiSize(28), 15, Mint);
    text("FIND THE GREEN GATE", GetScreenWidth()-margin-MeasureText("FIND THE GREEN GATE",uiSize(14)), footer, 14, Muted);
    // Tiny aim marker is also useful for attaching in Zero Gravity.
    int cx = GetScreenWidth()/2, cy = GetScreenHeight()/2;
    Color aim = Fade(Paper, .45f);
    DrawLine(cx-uiSize(5), cy, cx-uiSize(2), cy, aim);
    DrawLine(cx+uiSize(2), cy, cx+uiSize(5), cy, aim);
    DrawLine(cx, cy-uiSize(5), cx, cy-uiSize(2), aim);
    DrawLine(cx, cy+uiSize(2), cx, cy+uiSize(5), aim);
}

void Renderer::drawMinimap(const Game& game) {
    using namespace MenuTheme;
    int size = game.maze().size();
    int cell = std::max(1, uiSize(164)/size);
    int board = size*cell;
    int padding = uiSize(14), margin = uiSize(22);
    int panelWidth = board+padding*2;
    int panelX = GetScreenWidth()-margin-panelWidth;
    hudPanel({(float)panelX, (float)margin, (float)panelWidth, (float)(board+uiSize(68))});
    int x = panelX+padding, y = margin+uiSize(35);
    DrawText("FIELD MAP", x, margin+uiSize(13), uiSize(12), Muted);
    const char* dimensions = TextFormat("%d",size);
    DrawText(dimensions, panelX+panelWidth-padding-MeasureText(dimensions,uiSize(12)), margin+uiSize(13), uiSize(12), Amber);
    DrawRectangle(x, y, board, board, {6, 15, 21, 255});
    for (int row = 0; row < size; ++row) for (int col = 0; col < size; ++col) {
        int seen = game.seenState(row,col);
        if (!seen) continue;
        char tile = game.maze().cell(row,col);
        Color color = tile == '#' ? Color{63, 97, 102, 255} : tile == 'E' ? Mint
            : seen == 2 ? Color{140, 117, 77, 255} : Color{25, 48, 56, 255};
        DrawRectangle(x+col*cell, y+row*cell, cell, cell, color);
    }
    float px = x+(game.x()+.5f)*cell, py = y+(game.z()+.5f)*cell;
    float radius = std::max(2.0f, cell*.48f);
    DrawCircleV({px,py}, radius, Amber);
    DrawLineEx({px,py}, {px+std::cos(game.yaw())*radius*3, py+std::sin(game.yaw())*radius*3},
               std::max(1.0f, uiScale()), Paper);
    const char* caption = "YOUR STEPS, YOUR MAP";
    // DrawText clamps to ten pixels; shorten the label instead of shrinking below it.
    int captionSize = std::max(10, uiSize(10));
    if (MeasureText(caption, captionSize) > board) caption = "YOUR ROUTE";
    DrawText(caption, x, y+board+uiSize(12), captionSize, Muted);
}
