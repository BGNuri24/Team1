#include "../logic/GravityObstacles.h"
#include "Renderer.h"
#include <cmath>

// Draw matching physical bulkheads and emissive hazard strips in separate lighting passes.
// Warning marks sit before both approaches, giving players time to switch to the clear surface.
void Renderer::drawObstacles(const Game& game, bool glow) {
    const float pulse = 0.76f + 0.24f * std::sin((float)GetTime() * 4.0f);
    for (const GravityObstacle& obstacle : game.obstacles()) {
        const BoundingBox box = gravityObstacleBounds(obstacle);
        const Vector3 center = {(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f,
                                (box.min.z + box.max.z) * 0.5f};
        if (std::fabs(center.x - game.x()) > 14 || std::fabs(center.z - game.z()) > 14)
            continue;
        const Vector3 size = {box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z};
        if (!glow) {
            DrawCubeV(center, size, palette.trim);
            DrawCubeWiresV(center, size, palette.accent);
            for (int side : {-1, 1}) {
                for (int stripe = 0; stripe < 7; ++stripe) {
                    Vector3 mark = center;
                    mark.y = obstacle.ceiling ? MazeGeometry::CeilingHeight - 0.008f : 0.008f;
                    const float across = (stripe - 3) * 0.23f;
                    mark.x += obstacle.acrossX ? across : side * 0.66f;
                    mark.z += obstacle.acrossX ? side * 0.66f : across;
                    DrawCubeV(mark,
                              obstacle.acrossX ? Vector3{0.13f, 0.008f, 0.18f}
                                               : Vector3{0.18f, 0.008f, 0.13f},
                              stripe % 2 ? palette.accent : palette.fog);
                }
            }
        } else {
            for (int face : {-1, 1}) {
                Vector3 light = center;
                light.x += obstacle.acrossX ? 0 : face * (GravityBarrierDepth * 0.5f + 0.002f);
                light.z += obstacle.acrossX ? face * (GravityBarrierDepth * 0.5f + 0.002f) : 0;
                DrawCubeV(light,
                          obstacle.acrossX ? Vector3{1.64f, 0.075f, 0.006f}
                                           : Vector3{0.006f, 0.075f, 1.64f},
                          {255, (unsigned char)(42 * pulse), (unsigned char)(30 * pulse), 255});
                for (int stripe = 0; stripe < 9; ++stripe) {
                    Vector3 panel = light;
                    panel.y += obstacle.ceiling ? -0.13f : 0.13f;
                    panel.x += obstacle.acrossX ? (stripe - 4) * 0.18f : 0;
                    panel.z += obstacle.acrossX ? 0 : (stripe - 4) * 0.18f;
                    DrawCubeV(panel,
                              obstacle.acrossX ? Vector3{0.105f, 0.055f, 0.007f}
                                               : Vector3{0.007f, 0.055f, 0.105f},
                              stripe % 2 ? Color{255, 191, 81, 255} : Color{58, 42, 35, 255});
                }
            }
        }
    }
}
