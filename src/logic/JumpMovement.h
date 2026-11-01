#pragma once

// Track floor-relative jump height and horizontal momentum separately from maze collision and camera
// orientation.
// Use MazeGeometry world units for a CS-inspired model with buffered jumps, air acceleration and landing
// friction.
class JumpMovement {
public:
    static constexpr float MaxSpeed = 7.0f;
    static constexpr float JumpBuffer = 0.08f;
    // Buffer a fresh jump request for up to JumpBuffer seconds while waiting for floor contact.
    void requestJump();
    // Advance floor-relative movement from horizontal wish input and a positive duration in seconds;
    // input longer than one is normalized.
    void step(float wishX, float wishZ, float dt);
    // Restore the grounded state and remove all momentum and queued jump input.
    void reset();
    // Clear the horizontal velocity component blocked by an X-axis collision.
    void blockX();
    // Clear the horizontal velocity component blocked by a Z-axis collision.
    void blockZ();
    // Return horizontal X velocity in world units per second.
    float velocityX() const { return vx; }
    // Return horizontal Z velocity in world units per second.
    float velocityZ() const { return vz; }
    // Return the vertical eye-position offset above the normal standing height, in world units.
    float height() const { return offset; }
    // Report whether the jump arc has reached the floor and another buffered takeoff is allowed.
    bool grounded() const { return onGround; }
private:
    float vx = 0, vz = 0, verticalSpeed = 0, offset = 0;
    float bufferedJump = 0;
    bool onGround = true;
    bool momentum = false;
};
