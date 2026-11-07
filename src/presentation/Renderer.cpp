#include "Renderer.h"
#include "../data/MazeGeometry.h"
#include "../data/Validation.h"
#include "MazeShaders.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <bit>
#include <utility>

namespace {
static_assert(MAX_SIZE <= MazeShaders::MaxMazeRows);
static_assert(MazeShaders::MaxMazeRows <= 64);
constexpr float VIEW_DISTANCE = 15.0f;
constexpr Color AMBER = {244, 190, 102, 255};

struct Corner {
    float x, y, z, u, v;
};
struct Wall {
    float lx, lz, rx, rz;
    int row, col, side;
};

// Convert authored decoration coordinates to world space: scale X/Z by cell width and Y by ceiling
// height.
Vector3 world(Vector3 point) {
    return {point.x * MazeGeometry::CellSize, point.y * MazeGeometry::CeilingHeight,
            point.z * MazeGeometry::CellSize};
}

// Draw a decoration cube after scaling its center and dimensions from authored cell coordinates
// into world space.
void detailCube(Vector3 point, float width, float height, float depth, Color color) {
    DrawCube(world(point), width * MazeGeometry::CellSize, height * MazeGeometry::CeilingHeight,
             depth * MazeGeometry::CellSize, color);
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
    emit(a, color);
    emit(b, color);
    emit(c, color);
    emit(d, color);
}

// Map wall-local horizontal and vertical bounds onto an exposed face, offsetting the strip to avoid
// depth overlap.
void wallStrip(const Wall& wall, float left, float right, float bottom, float top, Color color,
               float inset = 0.004f) {
    float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
    float x0 = wall.lx + dx * left - dz * inset;
    float z0 = wall.lz + dz * left + dx * inset;
    float x1 = wall.lx + dx * right - dz * inset;
    float z1 = wall.lz + dz * right + dx * inset;
    quad({x0, bottom, z0, left, 1 - bottom}, {x1, bottom, z1, right, 1 - bottom},
         {x1, top, z1, right, 1 - top}, {x0, top, z0, left, 1 - top}, color);
}

// Visit open cells within the renderer's bounded neighborhood and distance cutoff before issuing
// their draw callbacks.
template <typename Draw> void nearbyCells(const Game& game, Draw draw) {
    int reach = (int)VIEW_DISTANCE + 1;
    int size = game.maze().size();
    for (int row = std::max(0, game.row() - reach); row <= std::min(size - 1, game.row() + reach);
         ++row)
        for (int col = std::max(0, game.col() - reach);
             col <= std::min(size - 1, game.col() + reach); ++col) {
            if (game.maze().cell(row, col) == '#')
                continue;
            float dx = col - game.x() / MazeGeometry::CellSize;
            float dz = row - game.z() / MazeGeometry::CellSize;
            if (dx * dx + dz * dz > (VIEW_DISTANCE + 1) * (VIEW_DISTANCE + 1))
                continue;
            draw(row, col);
        }
}

// Enumerate passage edges adjacent to solid cells, supplying consistently oriented wall-face
// coordinates to the draw callback.
template <typename Draw> void exposedWalls(const Game& game, Draw draw) {
    nearbyCells(game, [&](int row, int col) {
        float x = (float)col, z = (float)row;
        if (game.maze().cell(row - 1, col) == '#')
            draw(Wall{x - .5f, z - .5f, x + .5f, z - .5f, row, col, 0});
        if (game.maze().cell(row + 1, col) == '#')
            draw(Wall{x + .5f, z + .5f, x - .5f, z + .5f, row, col, 1});
        if (game.maze().cell(row, col - 1) == '#')
            draw(Wall{x - .5f, z + .5f, x - .5f, z - .5f, row, col, 2});
        if (game.maze().cell(row, col + 1) == '#')
            draw(Wall{x + .5f, z - .5f, x + .5f, z + .5f, row, col, 3});
    });
}

// Submit a horizontal decorative quad with authored half-width and half-depth at the requested
// height.
void floorPatch(float x, float z, float y, float halfWidth, float halfDepth, Color color) {
    quad({x - halfWidth, y, z - halfDepth, 0, 0}, {x - halfWidth, y, z + halfDepth, 0, 1},
         {x + halfWidth, y, z + halfDepth, 1, 1}, {x + halfWidth, y, z - halfDepth, 1, 0}, color);
}

// Choose fixture cells deterministically from the row/column pattern and intersections with at
// least three open neighbors.
bool ceilingFixture(const Game& game, int row, int col) {
    int neighbours =
        (game.maze().cell(row - 1, col) != '#') + (game.maze().cell(row + 1, col) != '#') +
        (game.maze().cell(row, col - 1) != '#') + (game.maze().cell(row, col + 1) != '#');
    return (row + col) % 3 == 0 || neighbours >= 3;
}

bool wallFixture(const Wall& wall) {
    return (wall.row * 13 + wall.col * 7 + wall.side * 3) % 5 <= 1;
}

}

// Initialize the perspective camera, upload procedural textures and load shader uniform locations,
// falling back when the custom shader is unavailable.
Renderer::Renderer() {
    cam = Camera3D{};
    cam.up = {0, 1, 0};
    cam.fovy = 70;
    cam.projection = CAMERA_PERSPECTIVE;
    atmosphere = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    if (atmosphere.id != 0 && atmosphere.id != rlGetShaderIdDefault()) {
        eyeLocation = GetShaderLocation(atmosphere, "eyePosition");
        directionLocation = GetShaderLocation(atmosphere, "viewDirection");
        emissionLocation = GetShaderLocation(atmosphere, "emission");
        lightPositionsLocation = GetShaderLocation(atmosphere, "lightPositions");
        lightDirectionsLocation = GetShaderLocation(atmosphere, "lightDirections");
        lightExtentsLocation = GetShaderLocation(atmosphere, "lightExtents");
        lightCountLocation = GetShaderLocation(atmosphere, "lightCount");
        wallRowsLocation = GetShaderLocation(atmosphere, "wallRows");
        mazeSizeLocation = GetShaderLocation(atmosphere, "mazeSize");
        cellSizeLocation = GetShaderLocation(atmosphere, "cellSize");
        fixtureColorLocation = GetShaderLocation(atmosphere, "fixtureColor");
        fogColorLocation = GetShaderLocation(atmosphere, "fogColor");
        bounceColorLocation = GetShaderLocation(atmosphere, "bounceColor");
        roomHeightLocation = GetShaderLocation(atmosphere, "roomHeight");
    } else {
        atmosphere = {};
    }
}

// Unload owned textures and the custom shader while the application's graphics context still
// exists.
Renderer::~Renderer() {
    if (atmosphere.id)
        UnloadShader(atmosphere);
}

// Build replacement textures before releasing the previous run's resources, and avoid uploads
// during normal frames.
void Renderer::prepareMazeTheme(MazeTheme theme) {
    if (texturesReady && activeTheme == theme)
        return;
    rlDrawRenderBatchActive();
    const MazePalette& chosen = mazePalette(theme);
    Textures replacement = createTextures(chosen);
    if (!replacement.valid())
        return;
    tex = std::move(replacement);
    palette = chosen;
    activeTheme = theme;
    texturesReady = true;
}

// Copy the game's displayed camera pose, render lit geometry and emissive details, then draw the
// HUD and explored-area minimap.
void Renderer::draw(const Game& game) {
    prepareMazeTheme(game.theme());
    cam.position = game.position();
    const Vector3 direction = game.cameraDirection();
    cam.target = {cam.position.x + direction.x, cam.position.y + direction.y,
                  cam.position.z + direction.z};
    cam.up = game.cameraUp();

    const bool glow = atmosphere.id && bloom.begin();
    ClearBackground(palette.fog);
    BeginMode3D(cam);
    if (atmosphere.id) {
        float emission = 0;
        const float fixture[] = {palette.lamp.r / 255.0f, palette.lamp.g / 255.0f,
                                 palette.lamp.b / 255.0f};
        const float fog[] = {palette.fog.r / 255.0f, palette.fog.g / 255.0f,
                             palette.fog.b / 255.0f};
        const Color bounceColor = ColorLerp(palette.wall, palette.paper, .35f);
        const float bounce[] = {bounceColor.r / 255.0f, bounceColor.g / 255.0f,
                                bounceColor.b / 255.0f};
        const float roomHeight = MazeGeometry::CeilingHeight;
        SetShaderValue(atmosphere, fixtureColorLocation, fixture, SHADER_UNIFORM_VEC3);
        SetShaderValue(atmosphere, fogColorLocation, fog, SHADER_UNIFORM_VEC3);
        SetShaderValue(atmosphere, bounceColorLocation, bounce, SHADER_UNIFORM_VEC3);
        SetShaderValue(atmosphere, roomHeightLocation, &roomHeight, SHADER_UNIFORM_FLOAT);
        SetShaderValue(atmosphere, eyeLocation, &cam.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(atmosphere, directionLocation, &direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(atmosphere, emissionLocation, &emission, SHADER_UNIFORM_FLOAT);
        updateLights(game);
        BeginShaderMode(atmosphere);
    }
    drawMaze(game);
    drawDetails(game);
    drawExit(game, false);
    drawObstacles(game, false);
    // Submit queued surface geometry before enabling emission so it retains the lighting uniforms
    // used when it was queued.
    rlDrawRenderBatchActive();
    if (atmosphere.id) {
        float emission = 1;
        SetShaderValue(atmosphere, emissionLocation, &emission, SHADER_UNIFORM_FLOAT);
    }
    drawLights(game);
    drawExit(game, true);
    drawObstacles(game, true);
    if (atmosphere.id)
        EndShaderMode();
    EndMode3D();
    if (glow)
        bloom.end();

    const int width = GetScreenWidth(), height = GetScreenHeight();
    DrawRectangleGradientV(0, 0, width, uiSize(160), {3, 10, 15, 115}, {3, 10, 15, 0});
    DrawRectangleGradientV(0, height - uiSize(130), width, uiSize(130), {3, 10, 15, 0},
                           {3, 10, 15, 160});
    hud.draw(game, palette);
}

// Batch nearby floor, ceiling and exposed wall surfaces, followed by shallow wall trim, with
// textures and culling state managed explicitly.
void Renderer::drawMaze(const Game& game) {
    rlDisableBackfaceCulling();
    rlSetTexture(tex.wall().id);
    rlBegin(RL_QUADS);
    exposedWalls(game, [](const Wall& wall) {
        unsigned char brightness = wall.side < 2 ? 255 : 235;
        wallStrip(wall, 0, 1, 0, 1, {brightness, brightness, brightness, 255}, 0);
    });
    rlEnd();

    rlSetTexture(tex.floor().id);
    rlBegin(RL_QUADS);
    nearbyCells(game,
                [](int row, int col) { floorPatch((float)col, (float)row, 0, .5f, .5f, WHITE); });
    rlEnd();

    rlSetTexture(tex.ceiling().id);
    rlBegin(RL_QUADS);
    nearbyCells(game,
                [](int row, int col) { floorPatch((float)col, (float)row, 1, .5f, .5f, WHITE); });
    rlEnd();

    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    exposedWalls(game, [&](const Wall& wall) {
        // Keep shallow dado and crown rails aligned with the wallpaper's lower paneling and ceiling
        // edge.
        wallStrip(wall, 0, 1, .018f, .030f, palette.trim);
        wallStrip(wall, 0, 1, .296f, .321f, palette.trim);
        wallStrip(wall, 0, 1, .316f, .321f, ColorLerp(palette.accent, palette.paper, .18f), .006f);
        wallStrip(wall, 0, 1, .955f, .974f, palette.trim);
        wallStrip(wall, .992f, .999f, .03f, .955f, ColorLerp(palette.trim, palette.wall, .2f));
    });
    rlEnd();
    rlSetTexture(rlGetTextureIdDefault());
    // Submit the two-sided geometry before restoring backface culling; queued vertices do not
    // retain the previous culling state.
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// Upload the maze's wall masks once per layout, then select nearby ceiling panels and wall lamps.
// A bounded light list keeps the fragment shader's cost independent of the overall maze size.
void Renderer::updateLights(const Game& game) {
    using namespace MazeShaders;
    const int size = game.maze().size();
    if (size != lightMazeSize || game.seed() != lightMazeSeed) {
        int rows[MaxMazeRows * 2]{};
        for (int row = 0; row < std::min(size, MaxMazeRows); ++row)
            for (int col = 0; col < std::min(size, 64); ++col)
                if (game.maze().cell(row, col) == '#')
                    rows[row * 2 + col / 32] |= std::bit_cast<int>(std::uint32_t{1} << (col % 32));
        SetShaderValueV(atmosphere, wallRowsLocation, rows, SHADER_UNIFORM_IVEC2, MaxMazeRows);
        SetShaderValue(atmosphere, mazeSizeLocation, &size, SHADER_UNIFORM_INT);
        const float cellSize = MazeGeometry::CellSize;
        SetShaderValue(atmosphere, cellSizeLocation, &cellSize, SHADER_UNIFORM_FLOAT);
        lightMazeSize = size;
        lightMazeSeed = game.seed();
    }
    Vector4 positions[MaxLights]{}, directions[MaxLights]{}, extents[MaxLights]{};
    float distances[MaxLights]{};
    int count = 0;
    auto add = [&](Vector3 position, Vector3 facing, Vector3 halfAxis, float halfWidth,
                   float radius, float strength) {
        const float dx = position.x - cam.position.x, dy = position.y - cam.position.y,
                    dz = position.z - cam.position.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (distance > 49.0f)
            return;
        const float rank = distance * (facing.y < -.5f ? .75f : 1.0f);
        if (count == MaxLights && rank >= distances[MaxLights - 1])
            return;
        int index = std::min(count, MaxLights - 1);
        while (index > 0 && rank < distances[index - 1]) {
            positions[index] = positions[index - 1];
            directions[index] = directions[index - 1];
            extents[index] = extents[index - 1];
            distances[index] = distances[index - 1];
            --index;
        }
        positions[index] = {position.x, position.y, position.z, radius};
        directions[index] = {facing.x, facing.y, facing.z, strength};
        extents[index] = {halfAxis.x, halfAxis.y, halfAxis.z, halfWidth};
        distances[index] = rank;
        count = std::min(count + 1, MaxLights);
    };
    nearbyCells(game, [&](int row, int col) {
        if (!ceilingFixture(game, row, col))
            return;
        const bool alongZ =
            game.maze().cell(row - 1, col) != '#' || game.maze().cell(row + 1, col) != '#';
        const float length = .26f * MazeGeometry::CellSize;
        add(world({(float)col, .928f, (float)row}), {0, -1, 0},
            alongZ ? Vector3{0, 0, length} : Vector3{length, 0, 0}, .09f * MazeGeometry::CellSize,
            3.8f, 1.85f);
    });
    exposedWalls(game, [&](const Wall& wall) {
        if (!wallFixture(wall))
            return;
        const float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
        Vector3 center = world({(wall.lx + wall.rx) * .5f, .825f, (wall.lz + wall.rz) * .5f});
        center.x -= dz * .12f;
        center.z += dx * .12f;
        add(center, {-dz, 0, dx}, {dx * .27f, 0, dz * .27f}, .011f, 2.8f, 1.5f);
    });
    SetShaderValue(atmosphere, lightCountLocation, &count, SHADER_UNIFORM_INT);
    if (count) {
        SetShaderValueV(atmosphere, lightPositionsLocation, positions, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(atmosphere, lightDirectionsLocation, directions, SHADER_UNIFORM_VEC4,
                        count);
        SetShaderValueV(atmosphere, lightExtentsLocation, extents, SHADER_UNIFORM_VEC4, count);
    }
}

// Place decorative sign-atlas panels, conduits and grilles on nearby maze walls.
void Renderer::drawDetails(const Game& game) {
    rlDisableBackfaceCulling();
    rlSetTexture(tex.decals().id);
    rlBegin(RL_QUADS);
    exposedWalls(game, [&](const Wall& wall) {
        if (game.maze().cell(wall.row, wall.col) == 'E')
            return;
        int hash = wall.row * 53 + wall.col * 17 + wall.side * 13;
        bool welcome = wall.row == 0 && wall.col == 0 && wall.side == 0;
        if (!welcome && hash % 23 != 0)
            return;
        int slot = welcome ? 0 : (hash / 19) % 4;
        const float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
        float x0 = wall.lx + dx * .17f - dz * .008f, z0 = wall.lz + dz * .17f + dx * .008f;
        float x1 = wall.lx + dx * .83f - dz * .008f, z1 = wall.lz + dz * .83f + dx * .008f;
        float top = slot * .25f + .001f, bottom = (slot + 1) * .25f - .001f;
        quad({x0, .42f, z0, .001f, bottom}, {x1, .42f, z1, .999f, bottom},
             {x1, .64f, z1, .999f, top}, {x0, .64f, z0, .001f, top}, WHITE);
    });
    rlEnd();
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    exposedWalls(game, [&](const Wall& wall) {
        int hash = wall.row * 53 + wall.col * 17 + wall.side * 13;
        // Match the slim service rail and its mounting clips to the current wallpaper palette.
        wallStrip(wall, .03f, .97f, .718f, .727f, ColorLerp(palette.trim, palette.accent, .35f),
                  .007f);
        wallStrip(wall, .10f, .12f, .704f, .742f, palette.trim, .010f);
        wallStrip(wall, .88f, .90f, .704f, .742f, palette.trim, .010f);
        if (hash % 7 != 0 || hash % 19 <= 2)
            return;
        wallStrip(wall, .63f, .92f, .225f, .37f, palette.trim, .006f);
        wallStrip(wall, .62f, .93f, .22f, .229f, palette.accent, .009f);
        wallStrip(wall, .62f, .93f, .367f, .376f, palette.trim, .009f);
        for (int i = 0; i < 5; ++i)
            wallStrip(wall, .645f, .905f, .241f + i * .025f, .248f + i * .025f,
                      ColorLerp(palette.trim, palette.accent, .2f), .011f);
    });
    rlEnd();
    rlSetTexture(rlGetTextureIdDefault());
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// Render wall lights and ceiling fixtures during the emissive pass.
void Renderer::drawLights(const Game& game) {
    const Color accent = ColorLerp(palette.accent, palette.paper, .3f);
    rlDisableBackfaceCulling();
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    exposedWalls(game, [&](const Wall& wall) {
        wallStrip(wall, .09f, .91f, .097f, .103f, Fade(accent, .65f), .006f);
        if (!wallFixture(wall))
            return;
        const Color lamp = palette.lamp;
        wallStrip(wall, .31f, .69f, .785f, .865f, palette.trim, .006f);
        wallStrip(wall, .33f, .67f, .795f, .855f, Fade(lamp, .10f), .008f);
        wallStrip(wall, .35f, .65f, .817f, .833f, lamp, .010f);
        wallStrip(wall, .34f, .36f, .795f, .855f, palette.trim, .012f);
        wallStrip(wall, .64f, .66f, .795f, .855f, palette.trim, .012f);
    });
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();

    // Position ceiling diffuser bodies and supports close to the ceiling, clear of the attached
    // player's camera and collision radius.
    nearbyCells(game, [&](int row, int col) {
        if (!ceilingFixture(game, row, col))
            return;
        const float x = (float)col, z = (float)row;
        const bool alongZ =
            game.maze().cell(row - 1, col) != '#' || game.maze().cell(row + 1, col) != '#';
        const float w = alongZ ? .22f : .56f, d = alongZ ? .56f : .22f;
        detailCube({x, .955f, z}, w, .024f, d, palette.trim);
        detailCube({x, .940f, z}, w - .04f, .007f, d - .04f, palette.lamp);
        detailCube({x, .936f, z}, w - .07f, .003f, d - .07f,
                   ColorLerp(palette.lamp, palette.paper, .35f));
        const float sx = alongZ ? .055f : .19f, sz = alongZ ? .19f : .055f;
        for (float side : {-1.0f, 1.0f})
            detailCube({x + sx * side, .984f, z + sz * side}, .005f, .028f, .005f, palette.accent);
    });
}
