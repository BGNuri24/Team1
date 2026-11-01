#pragma once

// Floor movement lives here so jump and bunny-hop tuning stay independent of Game.
// Units match MazeGeometry. This is CS-style movement, not a CS2 physics replica.
class JumpMovement {
public:
    static constexpr float MaxSpeed = 7.0f;
    static constexpr float JumpBuffer = 0.08f;
    void requestJump();
    void step(float wishX, float wishZ, float dt);
    void reset();
    void blockX();
    void blockZ();
    float velocityX() const { return vx; }
    float velocityZ() const { return vz; }
    float height() const { return offset; }
    bool grounded() const { return onGround; }
private:
    float vx = 0, vz = 0, verticalSpeed = 0, offset = 0;
    float bufferedJump = 0;
    bool onGround = true;
    bool momentum = false;
};
