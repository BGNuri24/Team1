#include "../logic/Game.h"
#include "../logic/JumpMovement.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    constexpr float Tick = 1.0f / 120;
    // Throw the supplied message when the movement model violates a jump expectation. The shared
    // gameplay executable catches the exception and reports which invariant failed.
    void requireJump(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    // Measure horizontal momentum with the magnitude of the model's X/Z velocity. This read-only
    // helper lets friction and air-strafe tests compare speed independently of movement
    // direction.
    float speed(const JumpMovement& movement) {
        return std::hypot(movement.velocityX(), movement.velocityZ());
    }

    // Exercise takeoff, apex, landing, expired midair requests, and a jump buffered shortly
    // before ground contact. Verify ordinary walking and a correctly timed follow-up hop follow
    // their separate movement rules.
    void testJumpArcAndBuffer() {
        JumpMovement movement;
        movement.step(1, 0, Tick);
        requireJump(movement.velocityX() == MazeGeometry::MovementSpeed && movement.grounded(),
                    "walking must use the reduced walking speed");
        movement.requestJump();
        float apex = 0;
        for (int frame = 0; frame < 100; ++frame) {
            movement.step(0, 0, Tick);
            apex = std::max(apex, movement.height());
        }
        requireJump(apex > .4f && apex < .45f && movement.height() == 0 && movement.grounded(),
            "a jump must rise, respect ceiling clearance and land without auto hopping");

        movement.reset();
        movement.requestJump();
        movement.step(0, 0, Tick);
        // Request another jump immediately after takeoff to verify that the buffer expires before the
        // next landing.
        movement.requestJump();
        for (int frame = 0; frame < 90; ++frame) movement.step(0, 0, Tick);
        requireJump(movement.grounded(), "an early midair jump must not cause a double jump or delayed hop");

        movement.requestJump();
        for (int frame = 0; frame < 60; ++frame) movement.step(0, 0, Tick);
        requireJump(!movement.grounded() && movement.height() < .12f, "buffer fixture must be descending near landing");
        movement.requestJump();
        bool landed = false, hopped = false;
        for (int frame = 0; frame < 15; ++frame) {
            movement.step(0, 0, Tick);
            landed |= movement.grounded();
            hopped |= landed && !movement.grounded();
        }
        requireJump(hopped, "a jump pressed just before landing must chain into the next hop");
    }

    // Check that timed hops retain strafe gains, the speed cap is enforced, landing friction
    // stops motion, and collisions can clear an axis.
    void testAirStrafeAndFriction() {
        JumpMovement movement;
        movement.requestJump();
        movement.step(1, 0, Tick);
        for (int frame = 0; frame < 45; ++frame) {
            const float current = speed(movement);
            movement.step(-movement.velocityZ() / current, movement.velocityX() / current, Tick);
            requireJump(speed(movement) <= JumpMovement::MaxSpeed + .0001f,
                        "air strafing must respect the bunny-hop speed cap");
        }
        requireJump(speed(movement) > JumpMovement::TakeoffSpeed + .3f,
                    "turning and air strafing must build useful momentum");
        while (!movement.grounded()) movement.step(0, 0, Tick);
        const float landingSpeed = speed(movement);
        movement.requestJump();
        movement.step(0, 0, Tick);
        requireJump(!movement.grounded() && std::fabs(speed(movement) - landingSpeed) < .001f,
            "a correctly timed bunny hop must preserve landing momentum");
        while (!movement.grounded()) movement.step(0, 0, Tick);
        for (int frame = 0; frame < 180; ++frame) movement.step(0, 0, Tick);
        requireJump(speed(movement) < .001f, "landing without another jump must apply ground friction");

        movement.reset();
        movement.requestJump();
        movement.step(1, 0, Tick);
        for (int frame = 0; frame < 30; ++frame)
            movement.step(1, 0, Tick);
        requireJump(std::fabs(speed(movement) - JumpMovement::TakeoffSpeed) < .001f,
                    "holding forward alone must retain the original takeoff speed without "
                    "accumulating speed");
        for (int frame = 0; frame < 1200; ++frame) {
            if (movement.grounded()) movement.requestJump();
            const float current = speed(movement);
            movement.step(-movement.velocityZ() / current, movement.velocityX() / current, Tick);
            requireJump(speed(movement) <= JumpMovement::MaxSpeed + .0001f,
                "chained air strafes must never exceed the maximum movement speed");
        }
        requireJump(speed(movement) > JumpMovement::MaxSpeed - .05f,
            "successful strafing and hopping must be able to reach the speed cap");
        movement.blockX();
        requireJump(movement.velocityX() == 0, "a wall must clear momentum on the blocked axis");
        movement.reset();
        requireJump(movement.grounded() && movement.height() == 0 && speed(movement) == 0,
            "reset must clear buffered jumps, height and momentum");
    }

    // Verify the slower ordinary walking speed remains independent of takeoff speed and the
    // air-strafe acceleration curve. Also check that chained hops retain their momentum instead
    // of being rescaled to walking speed.
    void testSeparateWalkingSpeed() {
        for (Vector2 wish : {Vector2{1, 0}, {0, -1}, {1, 1}}) {
            JumpMovement movement;
            movement.step(wish.x, wish.y, Tick);
            requireJump(
                std::fabs(speed(movement) - 3.24f) < .001f,
                "ordinary walking must use 3.24 units per second, including diagonal movement");
            movement.requestJump();
            movement.step(wish.x, wish.y, Tick);
            requireJump(std::fabs(speed(movement) - 4.0f) < .001f,
                        "jumping from the slower walk must preserve the original takeoff speed");
            while (!movement.grounded())
                movement.step(wish.x, wish.y, Tick);
            for (int frame = 0; frame < 60; ++frame)
                movement.step(wish.x, wish.y, Tick);
            requireJump(std::fabs(speed(movement) - 3.24f) < .001f,
                        "landing without chaining another hop must settle back to the slower walk");
            movement.requestJump();
            movement.step(wish.x, wish.y, Tick);
            requireJump(
                std::fabs(speed(movement) - 4.0f) < .001f,
                "jumping again after returning to walking must retain the original takeoff speed");
        }

        JumpMovement chained;
        chained.requestJump();
        chained.step(1, 0, Tick);
        for (int frame = 1; frame <= 300; ++frame) {
            if (chained.grounded())
                chained.requestJump();
            const float current = speed(chained);
            chained.step(-chained.velocityZ() / current, chained.velocityX() / current, Tick);
            // The original 120 Hz model gains 0.4 units/s perpendicular to momentum each tick.
            const float originalSpeed = std::min(7.0f, std::sqrt(16.0f + .16f * frame));
            requireJump(std::fabs(speed(chained) - originalSpeed) < .001f,
                        "the entire original bunny-hop acceleration curve and seven-unit cap must "
                        "be preserved");
        }

        for (GameMode mode : {GameMode::Normal, GameMode::ZeroGravity}) {
            for (bool ceiling : {false, true}) {
                if (ceiling && mode != GameMode::ZeroGravity)
                    continue;
                Game game(25, 100, mode, 1234);
                if (ceiling) {
                    game.update({0, 0, 0, 0, 0, true});
                    game.update({0, 0, 0, 0, .25f, false});
                }
                const Vector3 before = game.position();
                game.update({1, 0, 0, 0, .1f, false});
                requireJump(
                    std::fabs(std::hypot(game.x() - before.x, game.z() - before.z) - .324f) < .001f,
                    "actual floor and ceiling walking must both use the reduced speed");
                if (!ceiling) {
                    const Vector3 takeoff = game.position();
                    game.update({1, 0, 0, 0, Tick, false, true});
                    const float distance = std::hypot(game.x() - takeoff.x, game.z() - takeoff.z);
                    requireJump(std::fabs(distance / Tick - 4.0f) < .001f &&
                                    game.y() > MazeGeometry::StandingEyeHeight,
                                "the game's jumping path must retain its original horizontal "
                                "takeoff speed");
                }
            }
        }
    }

    // Exercise the actual camera-height integration, slow-frame wall collision,
    // gravity-transition resets and ignored input after timeout.
    void testJumpInGame() {
        using namespace MazeGeometry;
        for (GameMode mode : {GameMode::Normal, GameMode::ZeroGravity}) {
            Game game(25, 100, mode);
            InputState input{};
            input.dt = Tick;
            input.jumpPressed = true;
            game.update(input);
            requireJump(game.y() > StandingEyeHeight,
                        "Space must raise the actual game camera from the floor");
            input.jumpPressed = false;
            for (int frame = 0; frame < 100; ++frame) {
                game.update(input);
                requireJump(game.y() <= CeilingHeight - SurfaceOffset + .001f &&
                    game.y() >= StandingEyeHeight && !game.surfaceAttached(),
                    "floor jumping must not enter ceilings, fixtures or change gravity");
            }
            requireJump(game.y() == StandingEyeHeight, "the game must land at its original standing height");
        }

        Game wall(25, 100);
        constexpr float pi = 3.14159265358979323846f;
        const float turn = std::remainder(-pi / 2 - wall.yaw(), 2 * pi);
        wall.update({0, 0, turn / .0025f, 0, 0, false});
        wall.update({1, 0, 0, 0, .75f, false, true});
        requireJump(wall.row() == 0 && wall.z() >= -HalfCell + PlayerRadius - .001f,
            "a jumping slow frame must not tunnel through the maze boundary");
        requireJump(wall.y() == StandingEyeHeight, "a slow frame must integrate the complete jump and landing");

        Game gravity(25, 100, GameMode::ZeroGravity);
        gravity.update({0, 0, 0, 0, Tick, false, true});
        gravity.update({0, 0, 0, -400, 0, false});
        gravity.update({0, 0, 0, 0, 0, true});
        gravity.update({0, 0, 0, 0, .25f, false, true});
        requireJump(gravity.surfaceAttached() && std::fabs(gravity.y() - (CeilingHeight - StandingEyeHeight)) < .001f,
            "attaching mid-jump must safely clear floor momentum and height");
        gravity.update({0, 0, 0, 0, .1f, false, true});
        requireJump(std::fabs(gravity.y() - (CeilingHeight - StandingEyeHeight)) < .001f,
            "Space must not launch the player out of an attached ceiling");
        gravity.update({0, 0, 0, 0, .25f, true});
        gravity.update({0, 0, 0, 0, .25f, false});
        requireJump(!gravity.surfaceAttached() && gravity.y() == StandingEyeHeight,
            "returning from the ceiling must clear pending jump input");

        Game finished(10, .01f);
        finished.update({0, 0, 0, 0, .02f, false, true});
        requireJump(finished.lost() && finished.y() == StandingEyeHeight,
            "finished games must ignore jump input");
    }

    // Simulate jump arcs at 30, 60, and 144 frames per second and compare their apex and final
    // grounded height. This catches integration changes that make jumping strongly dependent on
    // frame rate.
    void testFrameRates() {
        for (float rate : {30.0f, 60.0f, 144.0f}) {
            Game game(15, 100);
            float apex = 0;
            for (int frame = 0; frame < (int)rate; ++frame) {
                game.update({0, 0, 0, 0, 1 / rate, false, frame == 0});
                apex = std::max(apex, game.y());
            }
            requireJump(std::fabs(apex - (MazeGeometry::StandingEyeHeight + .4266667f)) < .006f &&
                game.y() == MazeGeometry::StandingEyeHeight,
                "jump height and landing must remain consistent at 30, 60 and 144 FPS");
        }
    }
}

// Run the standalone JumpMovement cases and their integration through Game from the shared
// gameplay executable. Fail through descriptive exceptions while leaving user input and
// persistent saves untouched.
void runJumpTests() {
    testJumpArcAndBuffer();
    testAirStrafeAndFriction();
    testSeparateWalkingSpeed();
    testJumpInGame();
    testFrameRates();
}
