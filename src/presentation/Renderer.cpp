#include "Renderer.h"
#include "../data/MazeGeometry.h"
#include "../data/Validation.h"
#include "CorridorStyle.h"
#include "MazeShaders.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

namespace {
static_assert(MAX_SIZE <= MazeShaders::MaxMazeRows);
static_assert(MazeShaders::MaxMazeRows <= 64);
constexpr float VIEW_DISTANCE = 15.0f;

// Darker wall pigments let the red/cream carpet lead, while separate gilt trim and lamps retain
// their highlights. Balance the honey stone toward warm charcoal rather than bright orange.
constexpr Color WallMaterialTint{180, 180, 180, 255};
constexpr Color MarbleMaterialTint{126, 139, 157, 255};

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
    const Vector3 center = world(point);
    const float x0 = center.x - width * MazeGeometry::CellSize * .5f;
    const float x1 = center.x + width * MazeGeometry::CellSize * .5f;
    const float y0 = center.y - height * MazeGeometry::CeilingHeight * .5f;
    const float y1 = center.y + height * MazeGeometry::CeilingHeight * .5f;
    const float z0 = center.z - depth * MazeGeometry::CellSize * .5f;
    const float z1 = center.z + depth * MazeGeometry::CellSize * .5f;
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlTexCoord2f(0, 0);
    for (Vector3 vertex :
         {Vector3{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {x1, y0, z0},
          {x0, y0, z0},        {x0, y1, z0}, {x1, y1, z0}, {x0, y1, z1}, {x1, y1, z1},
          {x1, y1, z0},        {x0, y1, z0}, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1},
          {x0, y0, z1},        {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1},
          {x0, y0, z0},        {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}})
        rlVertex3f(vertex.x, vertex.y, vertex.z);
    rlEnd();
}

// The wall texture supplies distant ornament. Keep its shallow relief only near the camera's
// view, while lighting still considers nearby sources in all directions.
bool detailVisible(const Camera3D& camera, Vector3 position) {
    const float dx = position.x - camera.position.x, dy = position.y - camera.position.y,
                dz = position.z - camera.position.z;
    const float distance2 = dx * dx + dy * dy + dz * dz;
    if (distance2 > 16.0f * 16.0f)
        return false;
    const Vector3 forward{camera.target.x - camera.position.x, camera.target.y - camera.position.y,
                          camera.target.z - camera.position.z};
    return dx * forward.x + dy * forward.y + dz * forward.z + 1.8f >= std::sqrt(distance2) * .45f;
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

Vector3 wallPoint(const Wall& wall, float across, float height, float depth) {
    const float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
    return world({wall.lx + dx * across - dz * depth, height, wall.lz + dz * across + dx * depth});
}

// Raised architectural pieces stay shallow enough to leave the playable corridor clear.
void wallBlock(const Wall& wall, float left, float right, float bottom, float top, float depth,
               Color color) {
    const float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
    const float middle = (left + right) * .5f;
    const Vector3 center{wall.lx + dx * middle - dz * depth * .5f, (bottom + top) * .5f,
                         wall.lz + dz * middle + dx * depth * .5f};
    detailCube(center, dx ? right - left : depth, top - bottom, dx ? depth : right - left, color);
}

// Map marble in world proportions across the front and returns of a shallow pilaster. Its caps
// are covered by the gilded molding pass; texture density stays stable on narrow column faces.
void marbleBlock(const Wall& wall, float left, float right, float bottom, float top, float depth) {
    const float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
    auto point = [&](float across, float height, float offset, float u) {
        return Corner{wall.lx + dx * across - dz * offset, height,
                      wall.lz + dz * across + dx * offset, u * MazeGeometry::CellSize / .8f,
                      1 - height * MazeGeometry::CeilingHeight / .8f};
    };
    quad(point(left, bottom, depth, left), point(right, bottom, depth, right),
         point(right, top, depth, right), point(left, top, depth, left), MarbleMaterialTint);
    quad(point(left, bottom, 0, left - depth), point(left, bottom, depth, left),
         point(left, top, depth, left), point(left, top, 0, left - depth), MarbleMaterialTint);
    quad(point(right, bottom, depth, right), point(right, bottom, 0, right + depth),
         point(right, top, 0, right + depth), point(right, top, depth, right), MarbleMaterialTint);
}

// Curved gilt arms support the two upright glass cups seen in the loading-screen corridor.
void sconceArm(const Wall& wall, int side, Color metal) {
    auto point = [&](float t) {
        const float u = 1 - t;
        const float across =
            .5f + side * (3 * u * u * t * .09f + 3 * u * t * t * .10f + t * t * t * .06f);
        const float height =
            u * u * u * .727f + 3 * u * u * t * .665f + 3 * u * t * t * .720f + t * t * t * .790f;
        return wallPoint(wall, across, height + (side > 0 ? .018f * t : 0),
                         .028f + .072f * std::sin(t * PI * .5f));
    };
    Vector3 previous = point(0);
    for (int segment = 1; segment <= 6; ++segment) {
        const Vector3 next = point(segment / 6.0f);
        DrawCylinderEx(previous, next, .007f, .007f, 6, metal);
        previous = next;
    }
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

// Join two border runs around the outside corner of a solid cell, using matching woven miters.
void carpetCorner(float x, float z, float inwardX, float inwardZ) {
    using namespace CarpetLayout;
    const Corner corner{x, 0, z, BorderStartU, 0};
    const Corner alongX{x + inwardX * BorderWidth, 0, z, BorderU, 0};
    const Corner inside{x + inwardX * BorderWidth, 0, z + inwardZ * BorderWidth, BorderU,
                        BorderWidth};
    const Corner alongZ{x, 0, z + inwardZ * BorderWidth, BorderU, 1};
    const Corner oppositeCorner{x, 0, z, BorderStartU, 1};
    const Corner oppositeInside{inside.x, 0, inside.z, BorderU, 1 - BorderWidth};
    if (inwardX * inwardZ < 0) {
        quad(corner, alongX, inside, inside, WHITE);
        quad(oppositeCorner, oppositeInside, alongZ, alongZ, WHITE);
    } else {
        quad(corner, inside, alongX, alongX, WHITE);
        quad(oppositeCorner, alongZ, oppositeInside, oppositeInside, WHITE);
    }
}

// Crop the center field to the inner edge of each wall border. The patches share their edges;
// they never overlap, avoiding both coplanar flicker and shading the same floor pixels twice.
void carpetField(const Game& game, int row, int col) {
    using namespace CarpetLayout;
    const float left = game.maze().cell(row, col - 1) == '#' ? BorderWidth : 0;
    const float right = game.maze().cell(row, col + 1) == '#' ? BorderWidth : 0;
    const float top = game.maze().cell(row - 1, col) == '#' ? BorderWidth : 0;
    const float bottom = game.maze().cell(row + 1, col) == '#' ? BorderWidth : 0;
    const bool nw = !left && !top && game.maze().cell(row - 1, col - 1) == '#';
    const bool ne = !right && !top && game.maze().cell(row - 1, col + 1) == '#';
    const bool sw = !left && !bottom && game.maze().cell(row + 1, col - 1) == '#';
    const bool se = !right && !bottom && game.maze().cell(row + 1, col + 1) == '#';
    auto patch = [&](float u0, float v0, float u1, float v1) {
        quad({col - .5f + u0, 0, row - .5f + v0, u0 * FieldU, v0},
             {col - .5f + u0, 0, row - .5f + v1, u0 * FieldU, v1},
             {col - .5f + u1, 0, row - .5f + v1, u1 * FieldU, v1},
             {col - .5f + u1, 0, row - .5f + v0, u1 * FieldU, v0}, WHITE);
    };
    if (!nw && !ne && !sw && !se) {
        patch(left, top, 1 - right, 1 - bottom);
        return;
    }
    const float edges[] = {0, BorderWidth, 1 - BorderWidth, 1};
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) {
            if ((x == 0 && left) || (x == 2 && right) || (y == 0 && top) || (y == 2 && bottom))
                continue;
            if ((x == 0 && y == 0 && nw) || (x == 2 && y == 0 && ne) || (x == 0 && y == 2 && sw) ||
                (x == 2 && y == 2 && se))
                continue;
            patch(edges[x], edges[y], edges[x + 1], edges[y + 1]);
        }
    if (nw)
        carpetCorner(col - .5f, row - .5f, 1, 1);
    if (ne)
        carpetCorner(col + .5f, row - .5f, -1, 1);
    if (sw)
        carpetCorner(col - .5f, row + .5f, 1, -1);
    if (se)
        carpetCorner(col + .5f, row + .5f, -1, -1);
}

// Lay the woven border beside real walls, trimming its inner ends into a shared miter at corners.
// No transverse border is drawn at cell boundaries, so straight corridors read as one runner.
void carpetBorder(const Game& game, const Wall& wall) {
    using namespace CarpetLayout;
    const float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
    const bool leftCorner = game.maze().cell(wall.row - (int)dz, wall.col - (int)dx) == '#';
    const bool rightCorner = game.maze().cell(wall.row + (int)dz, wall.col + (int)dx) == '#';
    const float start = leftCorner ? BorderWidth : 0, end = rightCorner ? BorderWidth : 0;
    const float innerAX = wall.lx - dz * BorderWidth + dx * start;
    const float innerAZ = wall.lz + dx * BorderWidth + dz * start;
    const float innerBX = wall.rx - dz * BorderWidth - dx * end;
    const float innerBZ = wall.rz + dx * BorderWidth - dz * end;
    constexpr float y = 0;
    quad({wall.lx, y, wall.lz, BorderStartU, 0}, {innerAX, y, innerAZ, BorderU, start},
         {innerBX, y, innerBZ, BorderU, 1 - end}, {wall.rx, y, wall.rz, BorderStartU, 1}, WHITE);
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
        materialLocation = GetShaderLocation(atmosphere, "materialParams");
        wallSurfaceLocation = GetShaderLocation(atmosphere, "wallSurface");
        hdrLocation = GetShaderLocation(atmosphere, "hdrOutput");
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

    const bool glow = atmosphere.id && bloom.begin(true);
    ClearBackground(palette.fog);
    BeginMode3D(cam);
    if (atmosphere.id) {
        float emission = 0;
        const float hdr = glow ? 1.0f : 0.0f;
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
        SetShaderValue(atmosphere, hdrLocation, &hdr, SHADER_UNIFORM_FLOAT);
        updateLights(game);
        BeginShaderMode(atmosphere);
    }
    drawMaze(game);
    drawDetails(game);
    material(.38f, .08f, .0002f, .20f);
    drawExit(game, false);
    material(.33f, .55f, 0, .10f);
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
    DrawRectangleGradientV(0, 0, width, uiSize(160), Fade(palette.fog, .45f), Fade(palette.fog, 0));
    DrawRectangleGradientV(0, height - uiSize(130), width, uiSize(130), Fade(palette.fog, 0),
                           Fade(palette.fog, .63f));
    hud.draw(game, palette);
}

// Flush a material's vertices before changing the uniforms used by the next geometry group.
void Renderer::material(float roughness, float metalness, float relief, float clearCoat,
                        bool wallSurface) {
    if (!atmosphere.id)
        return;
    rlDrawRenderBatchActive();
    const float values[] = {roughness, metalness, relief, clearCoat};
    const int wall = wallSurface ? 1 : 0;
    SetShaderValue(atmosphere, materialLocation, values, SHADER_UNIFORM_VEC4);
    SetShaderValue(atmosphere, wallSurfaceLocation, &wall, SHADER_UNIFORM_INT);
}

// Batch nearby floor, ceiling and exposed wall surfaces, followed by shallow wall trim, with
// textures and culling state managed explicitly.
void Renderer::drawMaze(const Game& game) {
    rlDisableBackfaceCulling();
    material(.84f, 0, .0012f, 0, true);
    rlSetTexture(tex.wall().id);
    rlBegin(RL_QUADS);
    exposedWalls(game, [](const Wall& wall) {
        unsigned char brightness = wall.side < 2 ? 255 : 235;
        wallStrip(wall, 0, 1, 0, 1,
                  ColorTint(WallMaterialTint, {brightness, brightness, brightness, 255}), 0);
    });
    rlEnd();

    material(.96f, 0, .0008f);
    rlSetTexture(tex.floor().id);
    rlBegin(RL_QUADS);
    nearbyCells(game, [&](int row, int col) { carpetField(game, row, col); });
    exposedWalls(game, [&](const Wall& wall) { carpetBorder(game, wall); });
    rlEnd();

    material(.72f, 0, .00025f, .02f);
    rlSetTexture(tex.ceiling().id);
    rlBegin(RL_QUADS);
    nearbyCells(game,
                [](int row, int col) { floorPatch((float)col, (float)row, 1, .5f, .5f, WHITE); });
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
        // A square area approximates the round frosted bowl without changing the light budget.
        add(world({(float)col, .932f, (float)row}), {0, -1, 0},
            {.12f * MazeGeometry::CellSize, 0, 0}, .11f * MazeGeometry::CellSize, 3.8f, 1.85f);
    });
    exposedWalls(game, [&](const Wall& wall) {
        if (!wallFixture(wall))
            return;
        const float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
        Vector3 center = world({(wall.lx + wall.rx) * .5f, .820f, (wall.lz + wall.rz) * .5f});
        center.x -= dz * .23f;
        center.z += dx * .23f;
        add(center, {-dz, 0, dx},
            {dx * .07f * MazeGeometry::CellSize, 0, dz * .07f * MazeGeometry::CellSize},
            .035f * MazeGeometry::CellSize, 2.8f, 1.5f);
    });
    SetShaderValue(atmosphere, lightCountLocation, &count, SHADER_UNIFORM_INT);
    if (count) {
        SetShaderValueV(atmosphere, lightPositionsLocation, positions, SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(atmosphere, lightDirectionsLocation, directions, SHADER_UNIFORM_VEC4,
                        count);
        SetShaderValueV(atmosphere, lightExtentsLocation, extents, SHADER_UNIFORM_VEC4, count);
    }
}

// Layer classical skirting, cornices, a chair rail and fine panel frames over the wall material.
// Only the small entrance plaque uses the sign atlas; the corridors stay free of service clutter.
void Renderer::drawDetails(const Game& game) {
    const CorridorStyle style = corridorStyle(palette);
    rlDisableBackfaceCulling();
    material(.68f, 0, 0);
    rlSetTexture(tex.decals().id);
    rlBegin(RL_QUADS);
    exposedWalls(game, [&](const Wall& wall) {
        bool welcome = wall.row == 0 && wall.col == 0 && wall.side == 0;
        if (!welcome)
            return;
        const float dx = wall.rx - wall.lx, dz = wall.rz - wall.lz;
        float x0 = wall.lx + dx * .35f - dz * .015f, z0 = wall.lz + dz * .35f + dx * .015f;
        float x1 = wall.lx + dx * .65f - dz * .015f, z1 = wall.lz + dz * .65f + dx * .015f;
        quad({x0, .47f, z0, .001f, .249f}, {x1, .47f, z1, .999f, .249f},
             {x1, .57f, z1, .999f, .001f}, {x0, .57f, z0, .001f, .001f}, WHITE);
    });
    rlEnd();
    material(.27f, 0, .00015f, .40f);
    rlSetTexture(tex.marble().id);
    rlBegin(RL_QUADS);
    exposedWalls(game, [&](const Wall& wall) {
        if (!detailVisible(cam, world({(wall.lx + wall.rx) * .5f, .5f, (wall.lz + wall.rz) * .5f})))
            return;
        for (float left : {0.0f, .925f}) {
            marbleBlock(wall, left, left + .075f, .060f, .925f, .033f);
            marbleBlock(wall, left, left + .075f, .065f, .280f, .045f);
        }
    });
    rlEnd();
    material(.28f, .78f, 0, .05f);
    rlSetTexture(rlGetTextureIdDefault());
    exposedWalls(game, [&](const Wall& wall) {
        if (!detailVisible(cam, world({(wall.lx + wall.rx) * .5f, .5f, (wall.lz + wall.rz) * .5f})))
            return;
        wallBlock(wall, 0, 1, .008f, .046f, .018f, style.trimShade);
        wallBlock(wall, 0, 1, .046f, .067f, .025f, style.trim);
        wallBlock(wall, 0, 1, .067f, .080f, .029f, style.trimLight);
        wallBlock(wall, 0, 1, .300f, .312f, .009f, style.trimShade);
        wallBlock(wall, 0, 1, .312f, .325f, .015f, style.trimLight);
        wallBlock(wall, 0, 1, .325f, .330f, .011f, style.brass);
        wallBlock(wall, 0, 1, .916f, .940f, .014f, style.trimShade);
        wallBlock(wall, 0, 1, .940f, .963f, .026f, style.trim);
        wallBlock(wall, 0, 1, .963f, .985f, .038f, style.trimLight);
        wallBlock(wall, 0, 1, .985f, 1.0f, .042f, style.trim);
        // Keep the authored beading and carved scroll corners visible on the wall panels.
        // Stepped bases and capitals give the marble columns the loading screen's silhouette.
        for (float left : {0.0f, .925f}) {
            const float right = left + .075f;
            wallBlock(wall, left, right, .022f, .040f, .056f, style.trimShade);
            wallBlock(wall, left, right, .040f, .060f, .060f, style.trim);
            wallBlock(wall, left, right, .060f, .069f, .052f, style.trimLight);
            wallBlock(wall, left, right, .275f, .290f, .054f, style.trimLight);
            wallBlock(wall, left, right, .290f, .310f, .064f, style.trim);
            wallBlock(wall, left, right, .310f, .322f, .055f, style.trimLight);
            wallBlock(wall, left, right, .915f, .930f, .046f, style.trimLight);
            wallBlock(wall, left, right, .930f, .954f, .058f, style.trim);
            wallBlock(wall, left, right, .954f, .963f, .065f, style.trimLight);
            wallBlock(wall, left + .006f, left + .010f, .322f, .915f, .036f, style.trim);
            wallBlock(wall, right - .010f, right - .006f, .322f, .915f, .036f, style.trim);
        }
        if (wallFixture(wall)) {
            wallBlock(wall, .484f, .516f, .670f, .840f, .018f, style.trimShade);
            wallBlock(wall, .489f, .511f, .680f, .830f, .024f, style.brass);
            DrawSphereEx(wallPoint(wall, .5f, .695f, .037f), .018f, 4, 8, style.brass);
            for (int side : {-1, 1}) {
                sconceArm(wall, side, style.brass);
                const float x = .5f + side * .06f, y = .790f + (side > 0 ? .018f : 0);
                DrawCylinderEx(wallPoint(wall, x, y - .006f, .100f),
                               wallPoint(wall, x, y + .004f, .100f), .023f * MazeGeometry::CellSize,
                               .019f * MazeGeometry::CellSize, 12, style.brass);
            }
        }
    });
    nearbyCells(game, [&](int row, int col) {
        const float x = (float)col, z = (float)row;
        if (!detailVisible(cam, world({x, .96f, z})))
            return;
        detailCube({x - .487f, .987f, z}, .026f, .026f, 1.0f, style.trim);
        detailCube({x, .987f, z - .487f}, 1.0f, .026f, .026f, style.trim);
        if (ceilingFixture(game, row, col)) {
            DrawCylinder(world({x, .989f, z}), .19f * MazeGeometry::CellSize,
                         .19f * MazeGeometry::CellSize, .012f, 20, style.ceiling);
            DrawCylinder(world({x, .977f, z}), .14f * MazeGeometry::CellSize,
                         .14f * MazeGeometry::CellSize, .014f, 20, style.trimShade);
            DrawCylinder(world({x, .966f, z}), .145f * MazeGeometry::CellSize,
                         .145f * MazeGeometry::CellSize, .012f, 20, style.brass);
        }
    });
    rlSetTexture(rlGetTextureIdDefault());
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// Twin flared opal-glass sconce cups match the loading-screen artwork's warm practical lighting.
// The bowls remain above the ceiling-walking camera and do not change collision or gameplay.
void Renderer::drawLights(const Game& game) {
    exposedWalls(game, [&](const Wall& wall) {
        if (!wallFixture(wall))
            return;
        if (!detailVisible(cam, world({(wall.lx + wall.rx) * .5f, .8f, (wall.lz + wall.rz) * .5f})))
            return;
        const Color glass = ColorLerp(palette.lamp, palette.paper, .14f);
        for (int side : {-1, 1}) {
            const float x = .5f + side * .06f, y = .790f + (side > 0 ? .018f : 0);
            DrawCylinderEx(wallPoint(wall, x, y + .004f, .100f),
                           wallPoint(wall, x, y + .035f, .100f), .017f * MazeGeometry::CellSize,
                           .034f * MazeGeometry::CellSize, 12, glass);
            DrawCylinderEx(wallPoint(wall, x, y + .035f, .100f),
                           wallPoint(wall, x, y + .068f, .100f), .034f * MazeGeometry::CellSize,
                           .043f * MazeGeometry::CellSize, 12,
                           ColorLerp(glass, palette.paper, .18f));
        }
    });
    nearbyCells(game, [&](int row, int col) {
        if (!ceilingFixture(game, row, col))
            return;
        const float x = (float)col, z = (float)row;
        if (!detailVisible(cam, world({x, .96f, z})))
            return;
        DrawCylinder(world({x, .953f, z}), .132f * MazeGeometry::CellSize,
                     .108f * MazeGeometry::CellSize, .023f, 20,
                     ColorLerp(palette.lamp, palette.paper, .25f));
        DrawCylinder(world({x, .939f, z}), .108f * MazeGeometry::CellSize,
                     .059f * MazeGeometry::CellSize, .015f, 20, palette.lamp);
        DrawCylinder(world({x, .932f, z}), .059f * MazeGeometry::CellSize,
                     .045f * MazeGeometry::CellSize, .006f, 20,
                     ColorLerp(palette.lamp, palette.paper, .38f));
    });
}
