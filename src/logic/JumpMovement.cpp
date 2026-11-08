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

// Clear horizontal and vertical velocity, jump height and buffered input, returning movement to the
// initial grounded state.
void JumpMovement::reset() { *this = JumpMovement{}; }
// Refresh the short jump buffer so a press shortly before landing can trigger the next takeoff.
void JumpMovement::requestJump() { bufferedJump = JumpBuffer; }
// Discard X-axis momentum after collision while preserving motion along the other horizontal axis.
void JumpMovement::blockX() { vx = 0; }
// Discard Z-axis momentum after collision while preserving motion along the other horizontal axis.
void JumpMovement::blockZ() { vz = 0; }

// Advance one movement interval in seconds: consume a grounded jump, apply ground or air acceleration,
// integrate height, and cap horizontal speed.
void JumpMovement::step(float wishX, float wishZ, float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    const float length = std::hypot(wishX, wishZ);
    if (length > 1) { wishX /= length; wishZ /= length; }

    // Project existing velocity onto the requested direction and add only the remaining allowed speed
    // along that direction.
    auto accelerate = [&](float wishSpeed, float acceleration) {
        const float remaining = wishSpeed - (vx * wishX + vz * wishZ);
        if (remaining <= 0)
            return;
        const float amount = std::min(remaining, acceleration * TakeoffSpeed * dt);
        vx += wishX * amount;
        vz += wishZ * amount;
    };

    if (onGround) {
        const bool takingOff = bufferedJump > 0;
        const bool walking = std::fabs(vx - wishX * MazeGeometry::MovementSpeed) < .001f &&
                             std::fabs(vz - wishZ * MazeGeometry::MovementSpeed) < .001f;
        if (!momentum || (takingOff && walking)) {
            // Ordinary walking is slower, but a fresh jump from walking starts at the original
            // takeoff speed. Chained hops retain their existing momentum without rescaling.
            const float baseSpeed = takingOff ? TakeoffSpeed : MazeGeometry::MovementSpeed;
            vx = wishX * baseSpeed;
            vz = wishZ * baseSpeed;
        }
        if (takingOff) {
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
        // Add air acceleration along the requested movement direction while retaining the existing
        // velocity vector.
        // Changing the view alone does not redirect momentum; steering through strafing can increase
        // speed up to MaxSpeed.
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
