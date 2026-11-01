#include "JumpMovement.h"
#include "../data/MazeGeometry.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float Gravity = 12.0f;
    constexpr float JumpImpulse = 3.2f;
    constexpr float GroundFriction = 6.0f;
    constexpr float GroundAcceleration = 40.0f;
    constexpr float AirAcceleration = 12.0f;
    constexpr float AirWishSpeed = 0.8f;
}

void JumpMovement::reset() { *this = JumpMovement{}; }
void JumpMovement::requestJump() { bufferedJump = JumpBuffer; }
void JumpMovement::blockX() { vx = 0; }
void JumpMovement::blockZ() { vz = 0; }

void JumpMovement::step(float wishX, float wishZ, float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    const float length = std::hypot(wishX, wishZ);
    if (length > 1) { wishX /= length; wishZ /= length; }

    auto accelerate = [&](float wishSpeed, float acceleration) {
        const float remaining = wishSpeed - (vx * wishX + vz * wishZ);
        if (remaining <= 0) return;
        const float amount = std::min(remaining, acceleration * MazeGeometry::MovementSpeed * dt);
        vx += wishX * amount;
        vz += wishZ * amount;
    };

    if (onGround) {
        if (!momentum) {
            // Ordinary walking keeps its original immediate start/stop response.
            vx = wishX * MazeGeometry::MovementSpeed;
            vz = wishZ * MazeGeometry::MovementSpeed;
        }
        if (bufferedJump > 0) {
            verticalSpeed = JumpImpulse;
            onGround = false;
            momentum = true;
            bufferedJump = 0;
        } else if (momentum) {
            const float speed = std::hypot(vx, vz);
            if (speed > 0) {
                const float kept = std::max(0.0f, speed - std::max(speed, 1.0f) * GroundFriction * dt) / speed;
                vx *= kept;
                vz *= kept;
            }
            accelerate(MazeGeometry::MovementSpeed, GroundAcceleration);
            if (wishX == 0 && wishZ == 0 && std::hypot(vx, vz) < .001f) {
                vx = vz = 0;
                momentum = false;
            }
        }
    }
    if (!onGround) {
        // Only accelerate along the wish direction. Turning while strafing builds
        // speed; looking elsewhere never rotates existing momentum for free.
        accelerate(AirWishSpeed, AirAcceleration);
        offset += verticalSpeed * dt - 0.5f * Gravity * dt * dt;
        verticalSpeed -= Gravity * dt;
        const float headroom = MazeGeometry::CeilingHeight - MazeGeometry::SurfaceOffset
            - MazeGeometry::StandingEyeHeight;
        if (offset > headroom) { offset = headroom; verticalSpeed = std::min(0.0f, verticalSpeed); }
        if (offset <= 0 && verticalSpeed <= 0) {
            offset = verticalSpeed = 0;
            onGround = true;
        }
    }
    const float speed = std::hypot(vx, vz);
    if (speed > MaxSpeed) { vx *= MaxSpeed / speed; vz *= MaxSpeed / speed; }
    bufferedJump = std::max(0.0f, bufferedJump - dt);
}
