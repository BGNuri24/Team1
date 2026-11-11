#include "../logic/GravityObstacles.h"
#include "Renderer.h"
#include <cmath>

// Draw the run's lethal barriers at the same world-space bounds used by collision, adding their
// visible hazard treatment. The glow flag selects emissive contribution without creating or
// moving obstacles.
void Renderer::DrawObstacles(const Game& CurrentGame, bool Glow) {
    const float Pulse = 0.76f + 0.24f * std::sin((float)GetTime() * 4.0f);
    for (const GravityObstacle& Obstacle : CurrentGame.Obstacles()) {
        const BoundingBox Box = GravityObstacleBounds(Obstacle);
        const Vector3 CurrentCenter = {(Box.min.x + Box.max.x) * 0.5f, (Box.min.y + Box.max.y) * 0.5f,
                                (Box.min.z + Box.max.z) * 0.5f};
        if (std::fabs(CurrentCenter.x - CurrentGame.X()) > 14 || std::fabs(CurrentCenter.z - CurrentGame.Z()) > 14)
            continue;
        const Vector3 CurrentSize = {Box.max.x - Box.min.x, Box.max.y - Box.min.y, Box.max.z - Box.min.z};
        if (!Glow) {
            DrawCubeV(CurrentCenter, CurrentSize, CurrentPalette.Trim);
            DrawCubeWiresV(CurrentCenter, CurrentSize, CurrentPalette.CurrentAccent);
            for (int CurrentSide : {-1, 1}) {
                for (int Stripe = 0; Stripe < 7; ++Stripe) {
                    Vector3 CurrentMark = CurrentCenter;
                    CurrentMark.y = Obstacle.Ceiling ? MazeGeometry::CeilingHeight - 0.008f : 0.008f;
                    const float Across = (Stripe - 3) * 0.23f;
                    CurrentMark.x += Obstacle.AcrossX ? Across : CurrentSide * 0.66f;
                    CurrentMark.z += Obstacle.AcrossX ? CurrentSide * 0.66f : Across;
                    DrawCubeV(CurrentMark,
                              Obstacle.AcrossX ? Vector3{0.13f, 0.008f, 0.18f}
                                               : Vector3{0.18f, 0.008f, 0.13f},
                              Stripe % 2 ? CurrentPalette.CurrentAccent : CurrentPalette.Fog);
                }
            }
        } else {
            for (int CurrentFace : {-1, 1}) {
                Vector3 Light = CurrentCenter;
                Light.x += Obstacle.AcrossX ? 0 : CurrentFace * (GravityBarrierDepth * 0.5f + 0.002f);
                Light.z += Obstacle.AcrossX ? CurrentFace * (GravityBarrierDepth * 0.5f + 0.002f) : 0;
                DrawCubeV(Light,
                          Obstacle.AcrossX ? Vector3{1.64f, 0.075f, 0.006f}
                                           : Vector3{0.006f, 0.075f, 1.64f},
                          {255, (unsigned char)(42 * Pulse), (unsigned char)(30 * Pulse), 255});
                for (int Stripe = 0; Stripe < 9; ++Stripe) {
                    Vector3 CurrentPanel = Light;
                    CurrentPanel.y += Obstacle.Ceiling ? -0.13f : 0.13f;
                    CurrentPanel.x += Obstacle.AcrossX ? (Stripe - 4) * 0.18f : 0;
                    CurrentPanel.z += Obstacle.AcrossX ? 0 : (Stripe - 4) * 0.18f;
                    DrawCubeV(CurrentPanel,
                              Obstacle.AcrossX ? Vector3{0.105f, 0.055f, 0.007f}
                                               : Vector3{0.007f, 0.055f, 0.105f},
                              Stripe % 2 ? Color{255, 191, 81, 255} : Color{58, 42, 35, 255});
                }
            }
        }
    }
}
