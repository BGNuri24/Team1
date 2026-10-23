#include "Renderer.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

static const float FOG_END = 12.0f;     // distance where everything fades to black (smaller = darker maze)

// one corner of a textured quad: position + texture coordinates
struct Corner { float x, y, z, u, v; };

// sends one vertex to the GPU, darker the farther it is from the player (fog)
static void emit(const Vector3& eye, float shade, const Corner& c) {
    float dx = c.x - eye.x, dy = c.y - eye.y, dz = c.z - eye.z;
    float f = (1.0f - std::sqrt(dx * dx + dy * dy + dz * dz) / FOG_END) * shade;
    if (f < 0.0f) f = 0.0f;
    unsigned char b = (unsigned char)(255.0f * f);
    rlColor4ub(b, b, b, 255);
    rlTexCoord2f(c.u, c.v);
    rlVertex3f(c.x, c.y, c.z);
}

static void quad(const Vector3& eye, float shade,
                 const Corner& a, const Corner& b, const Corner& c, const Corner& d) {
    emit(eye, shade, a);
    emit(eye, shade, b);
    emit(eye, shade, c);
    emit(eye, shade, d);
}

// A vertical wall face (1 high), from the viewer's left edge (lx,lz) to the right edge (rx,rz).
static void wallFace(const Vector3& eye, float shade, float lx, float lz, float rx, float rz) {
    quad(eye, shade,
         {lx, 0, lz, 0, 1}, {rx, 0, rz, 1, 1}, {rx, 1, rz, 1, 0}, {lx, 1, lz, 0, 0});
}

Renderer::Renderer() {
    cam = Camera3D{};
    cam.up = {0, 1, 0};
    cam.fovy = 70;
    cam.projection = CAMERA_PERSPECTIVE;
    tex = createTextures();
}

Renderer::~Renderer() {
    destroyTextures(tex);
}

void Renderer::draw(const Game& game) {
    cam.position = {game.x(), game.y(), game.z()};
    Vector3 direction = game.cameraDirection();
    cam.target = {cam.position.x + direction.x,
                  cam.position.y + direction.y,
                  cam.position.z + direction.z};
    cam.up = game.cameraUp();

    ClearBackground(BLACK);
    BeginMode3D(cam);
    drawMaze(game);
    EndMode3D();

    drawMinimap(game);

    int t = (int)std::ceil(game.timeLeft());
    int margin = uiSize(20);
    int hudSize = uiSize(50);
    DrawText(TextFormat("%02d:%02d", t / 60, t % 60), margin, margin,
             hudSize, t <= 30 ? RED : WHITE);
    DrawText(game.surfaceAttached() ? "G - return to floor" : "Aim at wall/roof + G",
             margin, GetScreenHeight() - uiSize(62), uiSize(22), Fade(WHITE, 0.8f));
    DrawText("ESC - pause", margin, GetScreenHeight() - uiSize(34),
             uiSize(22), Fade(WHITE, 0.8f));
}

void Renderer::drawMaze(const Game& game) {
    const MazeData& m = game.maze();
    int n = m.size();
    Vector3 eye = cam.position;

    // only the cells near the player matter, everything farther is lost in the fog
    int reach = (int)FOG_END + 1;
    int r0 = game.row() - reach, r1 = game.row() + reach;
    int c0 = game.col() - reach, c1 = game.col() + reach;
    if (r0 < 0) r0 = 0;
    if (c0 < 0) c0 = 0;
    if (r1 > n - 1) r1 = n - 1;
    if (c1 > n - 1) c1 = n - 1;

    rlDisableBackfaceCulling();

    // walls: for every open cell, draw the walls around it
    rlSetTexture(tex.wall.id);
    rlBegin(RL_QUADS);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) {
            if (m.cell(r, c) == '#') continue;
            float x = (float)c, z = (float)r;
            if (m.cell(r - 1, c) == '#') wallFace(eye, 1.0f, x - 0.5f, z - 0.5f, x + 0.5f, z - 0.5f);   // north wall
            if (m.cell(r + 1, c) == '#') wallFace(eye, 1.0f, x + 0.5f, z + 0.5f, x - 0.5f, z + 0.5f);   // south wall
            if (m.cell(r, c - 1) == '#') wallFace(eye, 0.8f, x - 0.5f, z + 0.5f, x - 0.5f, z - 0.5f);   // west wall
            if (m.cell(r, c + 1) == '#') wallFace(eye, 0.8f, x + 0.5f, z - 0.5f, x + 0.5f, z + 0.5f);   // east wall
        }
    rlEnd();

    // floor and ceiling
    rlSetTexture(tex.floor.id);
    rlBegin(RL_QUADS);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) {
            if (m.cell(r, c) == '#') continue;
            float x = (float)c, z = (float)r;
            quad(eye, 0.9f, {x - 0.5f, 0, z - 0.5f, 0, 0}, {x - 0.5f, 0, z + 0.5f, 0, 1},
                            {x + 0.5f, 0, z + 0.5f, 1, 1}, {x + 0.5f, 0, z - 0.5f, 1, 0});   // floor
            quad(eye, 0.5f, {x - 0.5f, 1, z - 0.5f, 0, 0}, {x + 0.5f, 1, z - 0.5f, 1, 0},
                            {x + 0.5f, 1, z + 0.5f, 1, 1}, {x - 0.5f, 1, z + 0.5f, 0, 1});   // ceiling
        }
    rlEnd();
    rlSetTexture(0);
    rlEnableBackfaceCulling();

    // the exit: a green cube (fades into the fog like everything else)
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            if (m.cell(r, c) == 'E') {
                float dx = c - eye.x, dz = r - eye.z;
                float f = 1.0f - std::sqrt(dx * dx + dz * dz) / FOG_END;
                if (f < 0.0f) f = 0.0f;
                Vector3 pos = {(float)c, 0.25f, (float)r};
                DrawCube(pos, 0.5f, 0.5f, 0.5f, Color{0, (unsigned char)(230.0f * f), 0, 255});
                DrawCubeWires(pos, 0.5f, 0.5f, 0.5f, Color{0, (unsigned char)(90.0f * f), 0, 255});
            }
}

void Renderer::drawMinimap(const Game& game) {
    const MazeData& m = game.maze();
    int n = m.size();
    int cell = std::min(180, std::min(GetScreenWidth() / 4,
                                     GetScreenHeight() / 3)) / n;
    if (cell < 2) cell = 2;
    int size = n * cell;
    int margin = uiSize(14);
    int ox = GetScreenWidth() - size - margin;  // top-right corner
    int oy = margin;

    DrawRectangle(ox - 4, oy - 4, size + 8, size + 8, Color{15, 15, 20, 230});  // fog

    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            int s = game.seenState(r, c);
            if (s == 0) continue;               // undiscovered stays foggy
            Color col;
            if (m.cell(r, c) == '#')      col = LIGHTGRAY;
            else if (m.cell(r, c) == 'E') col = GREEN;
            else if (s == 2)              col = SKYBLUE;                // walked path
            else                          col = Color{60, 60, 70, 255}; // seen, not walked
            DrawRectangle(ox + c * cell, oy + r * cell, cell, cell, col);
        }

    float mx = ox + (game.x() + 0.5f) * cell;   // player marker
    float my = oy + (game.z() + 0.5f) * cell;
    float rad = cell / 2.0f < 2.0f ? 2.0f : cell / 2.0f;
    DrawCircle((int)mx, (int)my, rad, RED);
    DrawLine((int)mx, (int)my,
             (int)(mx + std::cos(game.yaw()) * cell * 1.5f),
             (int)(my + std::sin(game.yaw()) * cell * 1.5f), YELLOW);
}