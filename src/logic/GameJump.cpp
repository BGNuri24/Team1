#include "Game.h"
#include <algorithm>
#include <cmath>

// Update jumping and horizontal momentum while floor gravity is active, including the airborne part of a
// jump.
// Movement while attached to a wall or ceiling is handled by the surface-movement branch in
// Game::update.
bool Game::moveJumping(const InputState& input, const Vector3& wish) {
    if (input.jumpPressed) jumpMovement.requestJump();
    if (!std::isfinite(input.dt) || input.dt <= 0) return false;
    // Divide the frame into intervals of at most 1/120 second and check collision after each movement
    // step.
    // This keeps jump integration and the maximum bunny-hop displacement small enough to detect
    // intervening walls.
    const int parts = std::max(1, (int)std::ceil(input.dt * 120.0f));
    const float dt = input.dt / parts;
    for (int part = 0; part < parts; ++part) {
        jumpMovement.step(wish.x, wish.z, dt);
        py = MazeGeometry::StandingEyeHeight + jumpMovement.height();
        const float dx = jumpMovement.velocityX() * dt;
        const float dz = jumpMovement.velocityZ() * dt;
        const float oldX = px, oldZ = pz;
        if (moveHorizontal(dx, 1, true)) return true;
        if (std::fabs(px - oldX - dx) > .0001f) jumpMovement.blockX();
        if (moveHorizontal(dz, 1, false)) return true;
        if (std::fabs(pz - oldZ - dz) > .0001f) jumpMovement.blockZ();
    }
    return false;
}
