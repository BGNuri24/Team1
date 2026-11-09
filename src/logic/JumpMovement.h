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
    void requestJump();
    // Advance movement for a positive finite duration: normalize wish input, consume buffered
    // takeoff, apply ground friction or air acceleration, and integrate jump height. Respect
    // ceiling headroom, landing, and MaxSpeed while preserving chained-hop momentum.
    void step(float wishX, float wishZ, float dt);
    // Restore the initial grounded movement state, clearing horizontal momentum, vertical speed,
    // jump height, and buffered input. Gravity transfers use this to prevent a previous jump from
    // carrying into a new surface.
    void reset();
    // Discard horizontal velocity along X after a wall prevents that component of movement. Keep
    // Z velocity and vertical jump state so the player can continue sliding along the free axis.
    void blockX();
    // Discard horizontal velocity along Z after a wall prevents that component of movement. Keep
    // X velocity and vertical jump state so the player can continue sliding along the free axis.
    void blockZ();
    // Return horizontal X velocity in world units per second. Game converts it into a substep
    // displacement and can clear it with blockX when collision prevents that motion.
    float velocityX() const { return vx; }
    // Return horizontal Z velocity in world units per second. Game converts it into a substep
    // displacement and can clear it with blockZ when collision prevents that motion.
    float velocityZ() const { return vz; }
    // Return the current jump offset above normal standing eye height in world units. Game adds
    // this offset to StandingEyeHeight; zero represents a grounded player.
    float height() const { return offset; }
    // Report whether the jump arc has returned to the floor. step uses this state to apply ground
    // movement and consume buffered takeoff requests.
    bool grounded() const { return onGround; }
private:
    float vx = 0, vz = 0, verticalSpeed = 0, offset = 0;
    float bufferedJump = 0;
    bool onGround = true;
    bool momentum = false;
};
