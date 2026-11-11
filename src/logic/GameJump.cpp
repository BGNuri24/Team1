#include "Game.h"
#include <algorithm>
#include <cmath>

// Advance floor-relative jump and horizontal momentum in intervals no larger than 1/120 second,
// checking vertical and horizontal hazard motion each time. Clear velocity blocked by walls;
// return true when a lethal collision stops the update.
bool Game::MoveJumping(const InputState& CurrentInput, const Vector3& Wish) {
    if (CurrentInput.JumpPressed) CurrentJumpMovement.RequestJump();
    if (!std::isfinite(CurrentInput.Dt) || CurrentInput.Dt <= 0) return false;
    // Divide the frame into intervals of at most 1/120 second and check collision after each movement
    // step.
    // This keeps jump integration and the maximum bunny-hop displacement small enough to detect
    // intervening walls.
    const int Parts = std::max(1, (int)std::ceil(CurrentInput.Dt * 120.0f));
    const float Dt = CurrentInput.Dt / Parts;
    for (int Part = 0; Part < Parts; ++Part) {
        const Vector3 Previous = {Px, Py, Pz};
        CurrentJumpMovement.Step(Wish.x, Wish.z, Dt);
        Py = MazeGeometry::StandingEyeHeight + CurrentJumpMovement.Height();
        if (CheckObstacleCollision(Previous, {Px, Py, Pz}, false, false)) return true;
        const float Dx = CurrentJumpMovement.VelocityX() * Dt;
        const float Dz = CurrentJumpMovement.VelocityZ() * Dt;
        const float OldX = Px, OldZ = Pz;
        if (MoveHorizontal(Dx, 1, true)) return true;
        if (std::fabs(Px - OldX - Dx) > .0001f) CurrentJumpMovement.BlockX();
        if (MoveHorizontal(Dz, 1, false)) return true;
        if (std::fabs(Pz - OldZ - Dz) > .0001f) CurrentJumpMovement.BlockZ();
    }
    return false;
}
