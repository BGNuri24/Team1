#include "Game.h"
#include <algorithm>
#include <cmath>

// Called only while upright on the floor. Surface walking keeps its own rules.
bool Game::moveJumping(const InputState& input, const Vector3& wish) {
    if (input.jumpPressed) jumpMovement.requestJump();
    if (!std::isfinite(input.dt) || input.dt <= 0) return false;
    // Integrate air control, gravity and collision together, including slow frames.
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
