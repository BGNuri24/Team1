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

// Restore the initial grounded movement state, clearing horizontal momentum, vertical speed, jump
// height, and buffered input. Gravity transfers use this to prevent a previous jump from carrying
// into a new surface.
void JumpMovement::Reset() { *this = JumpMovement{}; }
// Refresh the jump-request buffer for JumpBuffer seconds. A request made shortly before landing
// can trigger the next takeoff when step observes ground contact.
void JumpMovement::RequestJump() { BufferedJump = JumpBuffer; }
// Discard horizontal velocity along X after a wall prevents that component of movement. Keep Z
// velocity and vertical jump state so the player can continue sliding along the free axis.
void JumpMovement::BlockX() { Vx = 0; }
// Discard horizontal velocity along Z after a wall prevents that component of movement. Keep X
// velocity and vertical jump state so the player can continue sliding along the free axis.
void JumpMovement::BlockZ() { Vz = 0; }

// Advance movement for a positive finite duration: normalize wish input, consume buffered
// takeoff, apply ground friction or air acceleration, and integrate jump height. Respect ceiling
// headroom, landing, and MaxSpeed while preserving chained-hop momentum.
void JumpMovement::Step(float WishX, float WishZ, float Dt) {
    if (!std::isfinite(Dt) || Dt <= 0) return;
    const float Length = std::hypot(WishX, WishZ);
    if (Length > 1) { WishX /= Length; WishZ /= Length; }

    // Project current momentum onto the normalized wish direction and add only the remaining
    // allowed speed along it. Ground and air branches share this acceleration rule with different
    // limits.
    auto Accelerate = [&](float WishSpeed, float Acceleration) {
        const float Remaining = WishSpeed - (Vx * WishX + Vz * WishZ);
        if (Remaining <= 0)
            return;
        const float Amount = std::min(Remaining, Acceleration * TakeoffSpeed * Dt);
        Vx += WishX * Amount;
        Vz += WishZ * Amount;
    };

    if (OnGround) {
        const bool TakingOff = BufferedJump > 0;
        const bool Walking = std::fabs(Vx - WishX * MazeGeometry::MovementSpeed) < .001f &&
                             std::fabs(Vz - WishZ * MazeGeometry::MovementSpeed) < .001f;
        if (!Momentum || (TakingOff && Walking)) {
            // Ordinary walking is slower, but a fresh jump from walking starts at the original
            // takeoff speed. Chained hops retain their existing momentum without rescaling.
            const float BaseSpeed = TakingOff ? TakeoffSpeed : MazeGeometry::MovementSpeed;
            Vx = WishX * BaseSpeed;
            Vz = WishZ * BaseSpeed;
        }
        if (TakingOff) {
            VerticalSpeed = JumpImpulse;
            OnGround = false;
            Momentum = true;
            BufferedJump = 0;
        } else if (Momentum) {
            const float Speed = std::hypot(Vx, Vz);
            if (Speed > 0) {
                const float Kept = std::max(0.0f, Speed - std::max(Speed, 1.0f) * GroundFriction * Dt) / Speed;
                Vx *= Kept;
                Vz *= Kept;
            }
            Accelerate(MazeGeometry::MovementSpeed, GroundAcceleration);
            if (WishX == 0 && WishZ == 0 && std::hypot(Vx, Vz) < .001f) {
                Vx = Vz = 0;
                Momentum = false;
            }
        }
    }
    if (!OnGround) {
        // Add air acceleration along the requested movement direction while retaining the existing
        // velocity vector.
        // Changing the view alone does not redirect momentum; steering through strafing can increase
        // speed up to MaxSpeed.
        Accelerate(AirWishSpeed, AirAcceleration);
        CurrentOffset += VerticalSpeed * Dt - 0.5f * Gravity * Dt * Dt;
        VerticalSpeed -= Gravity * Dt;
        const float Headroom = MazeGeometry::CeilingHeight - MazeGeometry::SurfaceOffset
            - MazeGeometry::StandingEyeHeight;
        if (CurrentOffset > Headroom) { CurrentOffset = Headroom; VerticalSpeed = std::min(0.0f, VerticalSpeed); }
        if (CurrentOffset <= 0 && VerticalSpeed <= 0) {
            CurrentOffset = VerticalSpeed = 0;
            OnGround = true;
        }
    }
    const float Speed = std::hypot(Vx, Vz);
    if (Speed > MaxSpeed) { Vx *= MaxSpeed / Speed; Vz *= MaxSpeed / Speed; }
    BufferedJump = std::max(0.0f, BufferedJump - Dt);
}
