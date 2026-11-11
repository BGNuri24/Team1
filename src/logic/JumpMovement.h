#pragma once

// Track floor-relative jump height and horizontal momentum separately from maze collision and camera
// orientation.
// Use MazeGeometry world units for a CS-inspired model with buffered jumps, air acceleration and landing
// friction.
class JumpMovement {
public:
    // Keep the original takeoff and acceleration scale independent of ordinary walking speed.
    static constexpr float TakeoffSpeed = 4.0f;
    static constexpr float MaxSpeed = 7.0f;
    static constexpr float JumpBuffer = 0.08f;
    // Refresh the jump-request buffer for JumpBuffer seconds. A request made shortly before
    // landing can trigger the next takeoff when step observes ground contact.
    void RequestJump();
    // Advance movement for a positive finite duration: normalize wish input, consume buffered
    // takeoff, apply ground friction or air acceleration, and integrate jump height. Respect
    // ceiling headroom, landing, and MaxSpeed while preserving chained-hop momentum.
    void Step(float WishX, float WishZ, float Dt);
    // Restore the initial grounded movement state, clearing horizontal momentum, vertical speed,
    // jump height, and buffered input. Gravity transfers use this to prevent a previous jump from
    // carrying into a new surface.
    void Reset();
    // Discard horizontal velocity along X after a wall prevents that component of movement. Keep
    // Z velocity and vertical jump state so the player can continue sliding along the free axis.
    void BlockX();
    // Discard horizontal velocity along Z after a wall prevents that component of movement. Keep
    // X velocity and vertical jump state so the player can continue sliding along the free axis.
    void BlockZ();
    // Return horizontal X velocity in world units per second. Game converts it into a substep
    // displacement and can clear it with blockX when collision prevents that motion.
    float VelocityX() const { return Vx; }
    // Return horizontal Z velocity in world units per second. Game converts it into a substep
    // displacement and can clear it with blockZ when collision prevents that motion.
    float VelocityZ() const { return Vz; }
    // Return the current jump offset above normal standing eye height in world units. Game adds
    // this offset to StandingEyeHeight; zero represents a grounded player.
    float Height() const { return CurrentOffset; }
    // Report whether the jump arc has returned to the floor. step uses this state to apply ground
    // movement and consume buffered takeoff requests.
    bool Grounded() const { return OnGround; }
private:
    float Vx = 0, Vz = 0, VerticalSpeed = 0, CurrentOffset = 0;
    float BufferedJump = 0;
    bool OnGround = true;
    bool Momentum = false;
};
