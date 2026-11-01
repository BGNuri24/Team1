#include "Renderer.h"
#include "Ui.h"
#include "rlgl.h"
#include "../data/MazeGeometry.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float VIEW_DISTANCE = 15.0f;
constexpr Color FOG = {7, 16, 21, 255};
constexpr Color AMBER = {244, 190, 102, 255};
constexpr Color MINT = {113, 216, 182, 255};
constexpr int MAX_CEILING_LIGHTS = 6;

// Embed the GLSL programs for interpolated ceiling-light spill, per-fragment view lighting and distance
// fog.
// Keeping the shader sources here avoids separate shader-file paths or runtime asset downloads.
const char* VERTEX_SHADER = R"glsl(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform vec3 ceilingLights[6];
uniform int ceilingLightCount;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 worldPosition;
out vec3 ceilingIllumination;
// Transform the vertex and pass texture coordinates, color, world position and ceiling-light
// contribution to the fragment stage.
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    worldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    // Calculate nearby fixture contributions at each vertex and interpolate their combined illumination
    // across the surface.
    // The fragment shader evaluates view lighting and fog separately, avoiding a fixture loop for every
    // fragment.
    ceilingIllumination = vec3(0.0);
    for (int i = 0; i < ceilingLightCount; ++i) {
        vec3 delta = ceilingLights[i] - worldPosition;
        ceilingIllumination += vec3(0.95, 0.69, 0.37) * 0.48 / (1.0 + dot(delta, delta)*1.4);
    }
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

const char* FRAGMENT_SHADER = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 worldPosition;
in vec3 ceilingIllumination;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 eyePosition;
uniform vec3 viewDirection;
uniform float emission;
out vec4 finalColor;
// Shade the sampled surface using ambient, view-directed and ceiling light, then blend distance fog
// while retaining material alpha.
void main() {
    vec4 surface = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 offset = worldPosition - eyePosition;
    float distance = length(offset);
    float cone = smoothstep(0.48, 0.98, dot(offset / max(distance, 0.001), viewDirection));
    float nearby = exp(-distance * 0.18);
    vec3 light = vec3(0.34, 0.43, 0.48) + nearby * vec3(0.30, 0.33, 0.30)
        + cone * exp(-distance * 0.10) * vec3(0.47, 0.40, 0.29);
    light += ceilingIllumination;
    vec3 lit = surface.rgb * mix(light, vec3(1.0), emission);
    float fog = smoothstep(5.0, 25.0, distance);
    finalColor = vec4(mix(lit, vec3(0.0275, 0.0627, 0.0824), fog), surface.a);
}
)glsl";

struct Corner { float x, y, z, u, v; };
struct Wall { float lx, lz, rx, rz; int row, col, side; };

// Convert authored decoration coordinates to world space: scale X/Z by cell width and Y by ceiling
// height.
Vector3 world(Vector3 point) {
    return {point.x*MazeGeometry::CellSize, point.y*MazeGeometry::CeilingHeight,
             point.z*MazeGeometry::CellSize};
}

// Draw a decoration cube after scaling its center and dimensions from authored cell coordinates into
// world space.
void detailCube(Vector3 point, float width, float height, float depth, Color color) {
    DrawCube(world(point), width*MazeGeometry::CellSize, height*MazeGeometry::CeilingHeight,
             depth*MazeGeometry::CellSize, color);
}

// Submit one vertex with its color and UV coordinates after converting its position to world units.
void emit(const Corner& point, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlTexCoord2f(point.u, point.v);
    Vector3 position = world({point.x, point.y, point.z});
    rlVertex3f(position.x, position.y, position.z);
}

// Submit four ordered corners to the currently active quad batch using a shared vertex color.
void quad(const Corner& a, const Corner& b, const Corner& c, const Corner& d, Color color) {
    emit(a, color); emit(b, color); emit(c, color); emit(d, color);
}

// Map wall-local horizontal and vertical bounds onto an exposed face, offsetting the strip to avoid
// depth overlap.
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

// Visit open cells within the renderer's bounded neighborhood and distance cutoff before issuing their
// draw callbacks.
template <typename Draw> void nearbyCells(const Game& game, Draw draw) {
    int reach = (int)VIEW_DISTANCE + 1;
    int size = game.maze().size();
    for (int row = std::max(0, game.row()-reach); row <= std::min(size-1, game.row()+reach); ++row)
        for (int col = std::max(0, game.col()-reach); col <= std::min(size-1, game.col()+reach); ++col) {
            if (game.maze().cell(row, col) == '#') continue;
            float dx = col - game.x()/MazeGeometry::CellSize;
            float dz = row - game.z()/MazeGeometry::CellSize;
            if (dx*dx + dz*dz > (VIEW_DISTANCE+1)*(VIEW_DISTANCE+1)) continue;
            draw(row, col);
        }
}

// Enumerate passage edges adjacent to solid cells, supplying consistently oriented wall-face
// coordinates to the draw callback.
template <typename Draw> void exposedWalls(const Game& game, Draw draw) {
    nearbyCells(game, [&](int row, int col) {
        float x = (float)col, z = (float)row;
        if (game.maze().cell(row-1, col) == '#') draw(Wall{x-.5f, z-.5f, x+.5f, z-.5f, row, col, 0});
        if (game.maze().cell(row+1, col) == '#') draw(Wall{x+.5f, z+.5f, x-.5f, z+.5f, row, col, 1});
        if (game.maze().cell(row, col-1) == '#') draw(Wall{x-.5f, z+.5f, x-.5f, z-.5f, row, col, 2});
        if (game.maze().cell(row, col+1) == '#') draw(Wall{x+.5f, z-.5f, x+.5f, z+.5f, row, col, 3});
    });
}

// Submit a horizontal decorative quad with authored half-width and half-depth at the requested height.
void floorPatch(float x, float z, float y, float halfWidth, float halfDepth, Color color) {
    quad({x-halfWidth,y,z-halfDepth,0,0}, {x-halfWidth,y,z+halfDepth,0,1},
         {x+halfWidth,y,z+halfDepth,1,1}, {x+halfWidth,y,z-halfDepth,1,0}, color);
}

// Choose fixture cells deterministically from the row/column pattern and intersections with at least
// three open neighbors.
bool ceilingFixture(const Game& game, int row, int col) {
    int neighbours = (game.maze().cell(row-1,col) != '#') + (game.maze().cell(row+1,col) != '#')
        + (game.maze().cell(row,col-1) != '#') + (game.maze().cell(row,col+1) != '#');
    return (row+col)%3 == 0 || neighbours >= 3;
}

// Expand a nonzero floor segment into a thin quad, using width as its perpendicular half-width.
void floorLine(float x0, float z0, float x1, float z1, float width, Color color) {
    float dx = x1-x0, dz = z1-z0;
    float length = std::sqrt(dx*dx+dz*dz);
    if (length <= .0001f) return;
    float nx = -dz/length*width, nz = dx/length*width;
    quad({x0+nx,.006f,z0+nz,0,0}, {x0-nx,.006f,z0-nz,0,1},
         {x1-nx,.006f,z1-nz,1,1}, {x1+nx,.006f,z1+nz,1,0}, color);
}

// Normalize an angle in radians to the signed interval around zero so compass offsets follow the
// shortest turn.
float wrapAngle(float angle) {
    return std::atan2(std::sin(angle), std::cos(angle));
}

// Draw the translucent rounded backing and border shared by in-game information panels.
void hudPanel(Rectangle bounds) {
    DrawRectangleRounded(bounds, 0.10f, 8, {9, 22, 29, 225});
    DrawRectangleRoundedLinesEx(bounds, 0.10f, 8, std::max(1.0f, uiScale()), {55, 80, 83, 175});
}
}

// Initialize the perspective camera, upload procedural textures and load shader uniform locations,
// falling back when the custom shader is unavailable.
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
        ceilingLightsLocation = GetShaderLocation(atmosphere, "ceilingLights");
        ceilingLightCountLocation = GetShaderLocation(atmosphere, "ceilingLightCount");
    } else {
        atmosphere = {};
    }
}

// Unload owned textures and the custom shader while the application's graphics context still exists.
Renderer::~Renderer() {
    destroyTextures(tex);
    if (atmosphere.id) UnloadShader(atmosphere);
}

// Copy the game's displayed camera pose, render lit geometry and emissive details, then draw the HUD
// and explored-area minimap.
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
        updateCeilingLights(game);
        BeginShaderMode(atmosphere);
    }
    drawMaze(game);
    drawDetails(game);
    // Submit queued surface geometry before enabling emission so it retains the lighting uniforms used
    // when it was queued.
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

// Batch nearby floor, ceiling and exposed wall surfaces, followed by shallow wall trim, with textures
// and culling state managed explicitly.
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

    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    exposedWalls(game, [](const Wall& wall) {
        // Draw shallow vertical ribs and base trim directly against each exposed wall's collision plane.
        wallStrip(wall, .015f, .043f, .03f, .97f, {48, 78, 82, 255});
        wallStrip(wall, .957f, .985f, .03f, .97f, {32, 54, 61, 255});
        wallStrip(wall, 0, 1, .025f, .065f, {21, 39, 45, 255});
        wallStrip(wall, 0, 1, .935f, .975f, {50, 75, 78, 255});
    });
    rlEnd();
    rlSetTexture(rlGetTextureIdDefault());
    // Submit the two-sided geometry before restoring backface culling; queued vertices do not retain the
    // previous culling state.
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// Select up to six nearby fixtures by squared camera distance and upload their world positions to the
// active atmosphere shader.
void Renderer::updateCeilingLights(const Game& game) {
    Vector3 positions[MAX_CEILING_LIGHTS]{};
    float distances[MAX_CEILING_LIGHTS]{};
    int count = 0;
    nearbyCells(game,[&](int row,int col) {
        if (!ceilingFixture(game,row,col)) return;
        Vector3 position = world({(float)col,.94f,(float)row});
        float dx = position.x-cam.position.x, dy = position.y-cam.position.y, dz = position.z-cam.position.z;
        float distance = dx*dx+dy*dy+dz*dz;
        if (count == MAX_CEILING_LIGHTS && distance >= distances[MAX_CEILING_LIGHTS-1]) return;
        int index = std::min(count,MAX_CEILING_LIGHTS-1);
        while (index > 0 && distance < distances[index-1]) {
            positions[index] = positions[index-1];
            distances[index] = distances[index-1];
            --index;
        }
        positions[index] = position;
        distances[index] = distance;
        count = std::min(count+1,MAX_CEILING_LIGHTS);
    });
    SetShaderValue(atmosphere,ceilingLightCountLocation,&count,SHADER_UNIFORM_INT);
    if (count) SetShaderValueV(atmosphere,ceilingLightsLocation,positions,SHADER_UNIFORM_VEC3,count);
}

// Place decorative sign-atlas panels, conduits, grilles and floor markings on nearby maze surfaces.
void Renderer::drawDetails(const Game& game) {
    rlDisableBackfaceCulling();
    rlSetTexture(tex.decals.id);
    rlBegin(RL_QUADS);
    exposedWalls(game,[&](const Wall& wall) {
        if (game.maze().cell(wall.row,wall.col) == 'E') return;
        int hash = wall.row*53 + wall.col*17 + wall.side*13;
        bool welcome = wall.row == 0 && wall.col == 0 && wall.side == 0;
        if (!welcome && hash%19 > 2) return;
        int slot = welcome ? 0 : (hash/19)%4;
        const float dx = wall.rx-wall.lx, dz = wall.rz-wall.lz;
        float x0 = wall.lx+dx*.17f-dz*.008f, z0 = wall.lz+dz*.17f+dx*.008f;
        float x1 = wall.lx+dx*.83f-dz*.008f, z1 = wall.lz+dz*.83f+dx*.008f;
        float top = slot*.25f+.001f, bottom = (slot+1)*.25f-.001f;
        quad({x0,.42f,z0,.001f,bottom},{x1,.42f,z1,.999f,bottom},
             {x1,.64f,z1,.999f,top},{x0,.64f,z0,.001f,top},WHITE);
    });
    rlEnd();
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    exposedWalls(game,[](const Wall& wall) {
        int hash = wall.row*53+wall.col*17+wall.side*13;
        // Place the horizontal copper conduit and its mounting clips above the decorative wall plaques.
        wallStrip(wall,.03f,.97f,.718f,.727f,{129,113,81,255},.007f);
        wallStrip(wall,.10f,.12f,.704f,.742f,{51,70,68,255},.010f);
        wallStrip(wall,.88f,.90f,.704f,.742f,{51,70,68,255},.010f);
        if (hash%7 != 0 || hash%19 <= 2) return;
        wallStrip(wall,.63f,.92f,.225f,.37f,{15,32,40,255},.006f);
        wallStrip(wall,.62f,.93f,.22f,.229f,{110,116,102,255},.009f);
        wallStrip(wall,.62f,.93f,.367f,.376f,{66,89,89,255},.009f);
        for (int i=0;i<5;++i)
            wallStrip(wall,.645f,.905f,.241f+i*.025f,.248f+i*.025f,{69,89,91,255},.011f);
    });
    nearbyCells(game,[&](int row,int col) {
        int neighbours = (game.maze().cell(row-1,col) != '#')+(game.maze().cell(row+1,col) != '#')
            +(game.maze().cell(row,col-1) != '#')+(game.maze().cell(row,col+1) != '#');
        float x = (float)col, z = (float)row;
        if (neighbours >= 3 || (row*7+col*3)%11 == 0) {
            for (int i=0;i<16;++i) {
                if (i%4 == 0) continue;
                float a = i*2*PI/16, b = (i+.76f)*2*PI/16;
                floorLine(x+std::cos(a)*.20f,z+std::sin(a)*.20f,
                          x+std::cos(b)*.20f,z+std::sin(b)*.20f,.004f,{147,133,98,200});
            }
            floorLine(x-.07f,z,x+.07f,z,.004f,{147,133,98,180});
            floorLine(x,z-.07f,x,z+.07f,.004f,{147,133,98,180});
        }
        if (neighbours == 1) {
            // Add service-hatch floor markings to cells with a single open neighbor; they do not point
            // toward the exit.
            for (int i=0;i<5;++i)
                floorLine(x-.17f+i*.06f,z+.31f,x-.14f+i*.06f,z+.25f,.006f,{158,126,72,200});
        }
    });
    rlEnd();
    rlSetTexture(rlGetTextureIdDefault());
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// Render visited-cell breadcrumbs, ceiling fixtures and the animated mint exit gateway during the
// emissive pass.
void Renderer::drawLights(const Game& game) {
    const Color accent = game.mode() == GameMode::ZeroGravity ? MINT : AMBER;
    const float time = (float)GetTime();
    rlDisableBackfaceCulling();
    rlSetTexture(rlGetTextureIdDefault());
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
        if (game.seenState(row, col) == 2) {
            // Draw floor breadcrumbs only in cells marked as visited so unexplored passages remain
            // undisclosed.
            for (int i = -1; i <= 1; ++i)
                floorPatch(x + i*.055f, z, .004f, .009f, .009f, Fade(accent, .62f));
        }
    });
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();

    // Position ceiling diffuser bodies and supports close to the ceiling, clear of the attached player's
    // camera and collision radius.
    nearbyCells(game, [&](int row, int col) {
        if (!ceilingFixture(game,row,col)) return;
        const float x = (float)col, z = (float)row;
        const bool alongZ = game.maze().cell(row-1,col) != '#' || game.maze().cell(row+1,col) != '#';
        const float w = alongZ ? .22f : .56f, d = alongZ ? .56f : .22f;
        detailCube({x,.955f,z},w,.024f,d,{36,51,51,255});
        detailCube({x,.940f,z},w-.04f,.007f,d-.04f,{255,220,158,255});
        detailCube({x,.936f,z},w-.07f,.003f,d-.07f,{255,240,195,255});
        const float sx = alongZ ? .055f : .19f, sz = alongZ ? .19f : .055f;
        for (float side : {-1.0f,1.0f})
            detailCube({x+sx*side,.984f,z+sz*side},.005f,.028f,.005f,{136,119,86,255});
    });

    // Draw the mint gateway at the exit cell when it is within the renderer's distance limit.
    const int last = game.maze().size()-1;
    float dx = last-game.x()/MazeGeometry::CellSize, dz = last-game.z()/MazeGeometry::CellSize;
    if (dx*dx + dz*dz > VIEW_DISTANCE*VIEW_DISTANCE) return;
    bool northEntrance = game.maze().cell(last-1, last) != '#';
    auto point = [last, northEntrance](float across, float y) -> Vector3 {
        return northEntrance ? Vector3{last+across, y, last+.31f}
                             : Vector3{last+.31f, y, last+across};
    };
    float pulse = .85f + .15f * std::sin(time*1.7f);
    for (float side : {-.34f, .34f}) {
        detailCube(point(side, .47f), .075f, .88f, .075f, {35, 76, 73, 255});
        Vector3 light = point(side, .47f);
        if (northEntrance) light.z -= .041f;
        else light.x -= .041f;
        detailCube(light, northEntrance ? .024f : .006f, .80f,
                 northEntrance ? .006f : .024f, MINT);
    }
    detailCube(point(0, .9f), northEntrance ? .75f : .08f, .07f, northEntrance ? .08f : .75f, MINT);
    detailCube(point(0, .06f), northEntrance ? .75f : .08f, .045f, northEntrance ? .08f : .75f, MINT);
    detailCube({(float)last, .012f, (float)last}, .72f, .018f, .72f, {31, 83, 71, 255});
    DrawCubeWires(world({(float)last,.025f,(float)last}),.73f*MazeGeometry::CellSize,
                   .02f*MazeGeometry::CeilingHeight,.73f*MazeGeometry::CellSize,MINT);
    for (int i = 0; i < 4; ++i) {
        float y = .22f + std::fmod(time*.12f + i*.18f, .55f);
        detailCube(point(0, y), northEntrance ? .54f : .008f, .007f,
                 northEntrance ? .008f : .54f, Fade(MINT, .12f*pulse));
    }
}

// Display the remaining time, mode, compass heading and applicable controls, with warning colors when
// time is short.
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

    // Project compass ticks relative to camera yaw, with east at +X and south at +Z in maze coordinates.
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
    // Draw a central reticle to show the look direction used when aiming Zero Gravity surface
    // attachment.
    int cx = GetScreenWidth()/2, cy = GetScreenHeight()/2;
    Color aim = Fade(Paper, .45f);
    DrawLine(cx-uiSize(5), cy, cx-uiSize(2), cy, aim);
    DrawLine(cx+uiSize(2), cy, cx+uiSize(5), cy, aim);
    DrawLine(cx, cy-uiSize(5), cx, cy-uiSize(2), aim);
    DrawLine(cx, cy+uiSize(2), cx, cy+uiSize(5), aim);
}

// Draw only revealed maze cells and the player's marker, distinguishing visited passages without
// disclosing the solution.
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
    float px = x+(game.x()/MazeGeometry::CellSize+.5f)*cell;
    float py = y+(game.z()/MazeGeometry::CellSize+.5f)*cell;
    float radius = std::max(2.0f, cell*.48f);
    DrawCircleV({px,py}, radius, Amber);
    DrawLineEx({px,py}, {px+std::cos(game.yaw())*radius*3, py+std::sin(game.yaw())*radius*3},
               std::max(1.0f, uiScale()), Paper);
    const char* caption = "YOUR STEPS, YOUR MAP";
    // Respect DrawText's ten-pixel minimum font size and switch to a shorter caption when the minimap
    // label would overflow.
    int captionSize = std::max(10, uiSize(10));
    if (MeasureText(caption, captionSize) > board) caption = "YOUR ROUTE";
    DrawText(caption, x, y+board+uiSize(12), captionSize, Muted);
}
