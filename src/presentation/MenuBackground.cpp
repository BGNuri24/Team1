#include "MenuBackground.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace {
constexpr Color INK = {6, 12, 19, 255};
constexpr Color AMBER = {244, 190, 102, 255};
constexpr Color MINT = {112, 229, 185, 255};
constexpr float WALL_HEIGHT = 0.65f;

// Use breadth-first search to reconstruct a start-to-exit route used only by the decorative background
// animation.
std::vector<MazeStep> solve(const MazeData& maze) {
    const int size = maze.size();
    std::vector<int> previous(size * size, -1);
    std::queue<int> pending;
    previous[0] = 0;
    pending.push(0);
    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};
    while (!pending.empty() && previous.back() == -1) {
        const int current = pending.front();
        pending.pop();
        for (int direction = 0; direction < 4; ++direction) {
            const int row = current / size + dr[direction];
            const int col = current % size + dc[direction];
            if (maze.cell(row, col) == '#') continue;
            const int next = row * size + col;
            if (previous[next] != -1) continue;
            previous[next] = current;
            pending.push(next);
        }
    }
    std::vector<MazeStep> result;
    if (previous.back() == -1) return result;
    for (int cell = size * size - 1; ; cell = previous[cell]) {
        result.push_back({cell / size, cell % size});
        if (cell == 0) break;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

// Convert a background-maze cell into a centered 3D position at the supplied decorative height.
Vector3 location(const MazeStep& step, float half, float height) {
    return {step.col - half, height, step.row - half};
}

// Submit one colored background-wall quad to the active rlgl geometry batch.
void face(Color color, Vector3 a, Vector3 b, Vector3 c, Vector3 d) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
    rlVertex3f(c.x, c.y, c.z);
    rlVertex3f(d.x, d.y, d.z);
}

// Draw the exposed wall faces around the background maze's passages using its centered decorative
// coordinates.
void drawWalls(const MazeData& maze, float half) {
    rlSetTexture(0);
    rlBegin(RL_QUADS);
    for (int row = 0; row < maze.size(); ++row) {
        for (int col = 0; col < maze.size(); ++col) {
            if (maze.cell(row, col) != '#') continue;
            const float x0 = col - half - 0.5f, x1 = x0 + 1.0f;
            const float z0 = row - half - 0.5f, z1 = z0 + 1.0f;
            const float h = WALL_HEIGHT;
            const unsigned char variation = (unsigned char)((row * 7 + col * 3) % 9);
            face({(unsigned char)(34 + variation), (unsigned char)(77 + variation),
                  (unsigned char)(85 + variation), 255},
                 {x0, h, z0}, {x0, h, z1}, {x1, h, z1}, {x1, h, z0});
            if (row == maze.size() - 1 || maze.cell(row + 1, col) != '#')
                face({18, 42, 49, 255}, {x0, 0, z1}, {x1, 0, z1}, {x1, h, z1}, {x0, h, z1});
            if (row == 0 || maze.cell(row - 1, col) != '#')
                face({16, 35, 44, 255}, {x1, 0, z0}, {x0, 0, z0}, {x0, h, z0}, {x1, h, z0});
            if (col == maze.size() - 1 || maze.cell(row, col + 1) != '#')
                face({25, 58, 65, 255}, {x1, 0, z1}, {x1, 0, z0}, {x1, h, z0}, {x1, h, z1});
            if (col == 0 || maze.cell(row, col - 1) != '#')
                face({14, 32, 40, 255}, {x0, 0, z0}, {x0, 0, z1}, {x0, h, z1}, {x0, h, z0});
        }
    }
    rlEnd();
}

// Draw a horizontal route segment between two background positions with the supplied width and color.
void pathSegment(Vector3 from, Vector3 to, float width, Color color) {
    DrawCube({(from.x + to.x) * 0.5f, from.y, (from.z + to.z) * 0.5f},
             std::fabs(to.x - from.x) + width, 0.025f,
             std::fabs(to.z - from.z) + width, color);
}

// Animate the route's traveling marker, fading trail and exit accent using elapsed animation time.
void drawRoute(const std::vector<MazeStep>& route, float half, float time) {
    if (route.size() < 2) return;
    const float progress = std::min(std::fmod(time * 5.0f, (float)route.size() + 24.0f),
                                    (float)route.size() - 1.0f);
    const int head = (int)progress;
    for (int index = 1; index < (int)route.size(); ++index) {
        const Vector3 from = location(route[index - 1], half, 0.045f);
        const Vector3 to = location(route[index], half, 0.045f);
        if (index > head) {
            pathSegment(from, to, 0.055f, {24, 51, 53, 255});
            continue;
        }
        const float trail = std::max(0.0f, 1.0f - (progress - index) / 18.0f);
        pathSegment(from, to, 0.2f, Fade(AMBER, 0.04f + trail * 0.12f));
        pathSegment(from, to, 0.075f, ColorLerp({105, 86, 55, 255}, AMBER, trail));
    }
    const Vector3 from = location(route[head], half, 0.11f);
    const Vector3 to = location(route[std::min(head + 1, (int)route.size() - 1)], half, 0.11f);
    const float blend = progress - head;
    const Vector3 runner = {from.x + (to.x - from.x) * blend, 0.13f,
                            from.z + (to.z - from.z) * blend};
    pathSegment({from.x, 0.05f, from.z}, {runner.x, 0.05f, runner.z}, 0.09f, AMBER);
    DrawSphereEx(runner, 0.14f, 6, 8, {255, 232, 174, 255});
    DrawCircle3D({runner.x, 0.035f, runner.z}, 0.32f + 0.04f * std::sin(time * 3),
                 {1, 0, 0}, 90, Fade(AMBER, 0.7f));
    DrawLine3D(runner, {runner.x, 1.2f, runner.z}, Fade(AMBER, 0.2f));

    const Vector3 exit = location(route.back(), half, 0);
    DrawCube({exit.x - 0.28f, 0.48f, exit.z}, 0.12f, 0.96f, 0.12f, MINT);
    DrawCube({exit.x + 0.28f, 0.48f, exit.z}, 0.12f, 0.96f, 0.12f, MINT);
    DrawCube({exit.x, 0.92f, exit.z}, 0.68f, 0.12f, 0.12f, MINT);
    DrawCircle3D({exit.x, 0.035f, exit.z}, 0.4f, {1, 0, 0}, 90,
                 Fade(MINT, 0.5f + 0.2f * std::sin(time * 2)));
}
}

// Generate a separate decorative maze and precompute the route followed by its animated marker.
MenuBackground::MenuBackground() : maze(29), route(solve(maze)) {}

// Orbit a perspective camera around the decorative maze, then apply screen-space shading to preserve
// menu-text contrast.
void MenuBackground::draw(float time) const {
    const float size = (float)maze.size();
    const float half = (size - 1.0f) * 0.5f;
    const float angle = 0.65f + time * 0.025f;
    // Shift the orbit's focal point to frame the animated maze beside the main-menu text.
    const Vector3 target = {-std::sin(angle) * size * 0.24f, 0,
                             std::cos(angle) * size * 0.24f};
    Camera3D camera{};
    camera.position = {target.x + std::cos(angle) * size * 0.82f,
                       size * (0.94f + 0.015f * std::sin(time * 0.15f)),
                       target.z + std::sin(angle) * size * 0.82f};
    camera.target = target;
    camera.up = {0, 1, 0};
    camera.fovy = 44;
    camera.projection = CAMERA_PERSPECTIVE;

    ClearBackground(INK);
    BeginMode3D(camera);
    DrawCube({0, -0.22f, 0}, size + 0.6f, 0.4f, size + 0.6f, {12, 25, 32, 255});
    DrawCubeWires({0, -0.22f, 0}, size + 0.6f, 0.4f, size + 0.6f, {39, 71, 75, 255});
    for (int row = 0; row < maze.size(); ++row) {
        for (int col = 0; col < maze.size(); ++col) {
            if (maze.cell(row, col) == '#') continue;
            const unsigned char shade = (unsigned char)(19 + (row + col) % 3);
            DrawCube({col - half, 0, row - half}, 0.96f, 0.02f, 0.96f,
                     {shade, (unsigned char)(shade + 12), (unsigned char)(shade + 16), 255});
        }
    }
    drawWalls(maze, half);
    drawRoute(route, half, time);
    EndMode3D();

    const int width = GetScreenWidth(), height = GetScreenHeight();
    DrawRectangleGradientH(0, 0, (int)(width * 0.72f), height,
                           {5, 11, 18, 252}, {5, 11, 18, 0});
    DrawRectangleGradientV(0, 0, width, height / 4, {5, 11, 18, 115}, {5, 11, 18, 0});
    DrawRectangleGradientV(0, height * 2 / 3, width, height - height * 2 / 3,
                           {5, 11, 18, 0}, {5, 11, 18, 210});
}
