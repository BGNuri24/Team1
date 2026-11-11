#include "../logic/Game.h"
#include "../logic/JumpMovement.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    constexpr float Tick = 1.0f / 120;
    // Throw the supplied message when the movement model violates a jump expectation. The shared
    // gameplay executable catches the exception and reports which invariant failed.
    void RequireJump(bool Value, const char* Message) {
        if (!Value) throw std::runtime_error(Message);
    }
    // Measure horizontal momentum with the magnitude of the model's X/Z velocity. This read-only
    // helper lets friction and air-strafe tests compare speed independently of movement
    // direction.
    float Speed(const JumpMovement& CurrentMovement) {
        return std::hypot(CurrentMovement.VelocityX(), CurrentMovement.VelocityZ());
    }

    // Exercise takeoff, apex, landing, expired midair requests, and a jump buffered shortly
    // before ground contact. Verify ordinary walking and a correctly timed follow-up hop follow
    // their separate movement rules.
    void TestJumpArcAndBuffer() {
        JumpMovement CurrentMovement;
        CurrentMovement.Step(1, 0, Tick);
        RequireJump(CurrentMovement.VelocityX() == MazeGeometry::MovementSpeed && CurrentMovement.Grounded(),
                    "walking must use the reduced walking speed");
        CurrentMovement.RequestJump();
        float Apex = 0;
        for (int Frame = 0; Frame < 100; ++Frame) {
            CurrentMovement.Step(0, 0, Tick);
            Apex = std::max(Apex, CurrentMovement.Height());
        }
        RequireJump(Apex > .4f && Apex < .45f && CurrentMovement.Height() == 0 && CurrentMovement.Grounded(),
            "a jump must rise, respect ceiling clearance and land without auto hopping");

        CurrentMovement.Reset();
        CurrentMovement.RequestJump();
        CurrentMovement.Step(0, 0, Tick);
        // Request another jump immediately after takeoff to verify that the buffer expires before the
        // next landing.
        CurrentMovement.RequestJump();
        for (int Frame = 0; Frame < 90; ++Frame) CurrentMovement.Step(0, 0, Tick);
        RequireJump(CurrentMovement.Grounded(), "an early midair jump must not cause a double jump or delayed hop");

        CurrentMovement.RequestJump();
        for (int Frame = 0; Frame < 60; ++Frame) CurrentMovement.Step(0, 0, Tick);
        RequireJump(!CurrentMovement.Grounded() && CurrentMovement.Height() < .12f, "buffer fixture must be descending near landing");
        CurrentMovement.RequestJump();
        bool Landed = false, Hopped = false;
        for (int Frame = 0; Frame < 15; ++Frame) {
            CurrentMovement.Step(0, 0, Tick);
            Landed |= CurrentMovement.Grounded();
            Hopped |= Landed && !CurrentMovement.Grounded();
        }
        RequireJump(Hopped, "a jump pressed just before landing must chain into the next hop");
    }

    // Check that timed hops retain strafe gains, the speed cap is enforced, landing friction
    // stops motion, and collisions can clear an axis.
    void TestAirStrafeAndFriction() {
        JumpMovement CurrentMovement;
        CurrentMovement.RequestJump();
        CurrentMovement.Step(1, 0, Tick);
        for (int Frame = 0; Frame < 45; ++Frame) {
            const float Current = Speed(CurrentMovement);
            CurrentMovement.Step(-CurrentMovement.VelocityZ() / Current, CurrentMovement.VelocityX() / Current, Tick);
            RequireJump(Speed(CurrentMovement) <= JumpMovement::MaxSpeed + .0001f,
                        "air strafing must respect the bunny-hop speed cap");
        }
        RequireJump(Speed(CurrentMovement) > JumpMovement::TakeoffSpeed + .3f,
                    "turning and air strafing must build useful momentum");
        while (!CurrentMovement.Grounded()) CurrentMovement.Step(0, 0, Tick);
        const float LandingSpeed = Speed(CurrentMovement);
        CurrentMovement.RequestJump();
        CurrentMovement.Step(0, 0, Tick);
        RequireJump(!CurrentMovement.Grounded() && std::fabs(Speed(CurrentMovement) - LandingSpeed) < .001f,
            "a correctly timed bunny hop must preserve landing momentum");
        while (!CurrentMovement.Grounded()) CurrentMovement.Step(0, 0, Tick);
        for (int Frame = 0; Frame < 180; ++Frame) CurrentMovement.Step(0, 0, Tick);
        RequireJump(Speed(CurrentMovement) < .001f, "landing without another jump must apply ground friction");

        CurrentMovement.Reset();
        CurrentMovement.RequestJump();
        CurrentMovement.Step(1, 0, Tick);
        for (int Frame = 0; Frame < 30; ++Frame)
            CurrentMovement.Step(1, 0, Tick);
        RequireJump(std::fabs(Speed(CurrentMovement) - JumpMovement::TakeoffSpeed) < .001f,
                    "holding forward alone must retain the original takeoff speed without "
                    "accumulating speed");
        for (int Frame = 0; Frame < 1200; ++Frame) {
            if (CurrentMovement.Grounded()) CurrentMovement.RequestJump();
            const float Current = Speed(CurrentMovement);
            CurrentMovement.Step(-CurrentMovement.VelocityZ() / Current, CurrentMovement.VelocityX() / Current, Tick);
            RequireJump(Speed(CurrentMovement) <= JumpMovement::MaxSpeed + .0001f,
                "chained air strafes must never exceed the maximum movement speed");
        }
        RequireJump(Speed(CurrentMovement) > JumpMovement::MaxSpeed - .05f,
            "successful strafing and hopping must be able to reach the speed cap");
        CurrentMovement.BlockX();
        RequireJump(CurrentMovement.VelocityX() == 0, "a wall must clear momentum on the blocked axis");
        CurrentMovement.Reset();
        RequireJump(CurrentMovement.Grounded() && CurrentMovement.Height() == 0 && Speed(CurrentMovement) == 0,
            "reset must clear buffered jumps, height and momentum");
    }

    // Verify the slower ordinary walking speed remains independent of takeoff speed and the
    // air-strafe acceleration curve. Also check that chained hops retain their momentum instead
    // of being rescaled to walking speed.
    void TestSeparateWalkingSpeed() {
        for (Vector2 Wish : {Vector2{1, 0}, {0, -1}, {1, 1}}) {
            JumpMovement CurrentMovement;
            CurrentMovement.Step(Wish.x, Wish.y, Tick);
            RequireJump(
                std::fabs(Speed(CurrentMovement) - 3.24f) < .001f,
                "ordinary walking must use 3.24 units per second, including diagonal movement");
            CurrentMovement.RequestJump();
            CurrentMovement.Step(Wish.x, Wish.y, Tick);
            RequireJump(std::fabs(Speed(CurrentMovement) - 4.0f) < .001f,
                        "jumping from the slower walk must preserve the original takeoff speed");
            while (!CurrentMovement.Grounded())
                CurrentMovement.Step(Wish.x, Wish.y, Tick);
            for (int Frame = 0; Frame < 60; ++Frame)
                CurrentMovement.Step(Wish.x, Wish.y, Tick);
            RequireJump(std::fabs(Speed(CurrentMovement) - 3.24f) < .001f,
                        "landing without chaining another hop must settle back to the slower walk");
            CurrentMovement.RequestJump();
            CurrentMovement.Step(Wish.x, Wish.y, Tick);
            RequireJump(
                std::fabs(Speed(CurrentMovement) - 4.0f) < .001f,
                "jumping again after returning to walking must retain the original takeoff speed");
        }

        JumpMovement CurrentChained;
        CurrentChained.RequestJump();
        CurrentChained.Step(1, 0, Tick);
        for (int Frame = 1; Frame <= 300; ++Frame) {
            if (CurrentChained.Grounded())
                CurrentChained.RequestJump();
            const float Current = Speed(CurrentChained);
            CurrentChained.Step(-CurrentChained.VelocityZ() / Current, CurrentChained.VelocityX() / Current, Tick);
            // The original 120 Hz model gains 0.4 units/s perpendicular to momentum each tick.
            const float OriginalSpeed = std::min(7.0f, std::sqrt(16.0f + .16f * Frame));
            RequireJump(std::fabs(Speed(CurrentChained) - OriginalSpeed) < .001f,
                        "the entire original bunny-hop acceleration curve and seven-unit cap must "
                        "be preserved");
        }

        for (GameMode Mode : {GameMode::Normal, GameMode::ZeroGravity}) {
            for (bool Ceiling : {false, true}) {
                if (Ceiling && Mode != GameMode::ZeroGravity)
                    continue;
                Game CurrentGame(25, 100, Mode, 1234);
                if (Ceiling) {
                    CurrentGame.Update({0, 0, 0, 0, 0, true});
                    CurrentGame.Update({0, 0, 0, 0, .25f, false});
                }
                const Vector3 Before = CurrentGame.Position();
                CurrentGame.Update({1, 0, 0, 0, .1f, false});
                RequireJump(
                    std::fabs(std::hypot(CurrentGame.X() - Before.x, CurrentGame.Z() - Before.z) - .324f) < .001f,
                    "actual floor and ceiling walking must both use the reduced speed");
                if (!Ceiling) {
                    const Vector3 Takeoff = CurrentGame.Position();
                    CurrentGame.Update({1, 0, 0, 0, Tick, false, true});
                    const float Distance = std::hypot(CurrentGame.X() - Takeoff.x, CurrentGame.Z() - Takeoff.z);
                    RequireJump(std::fabs(Distance / Tick - 4.0f) < .001f &&
                                    CurrentGame.Y() > MazeGeometry::StandingEyeHeight,
                                "the game's jumping path must retain its original horizontal "
                                "takeoff speed");
                }
            }
        }
    }

    // Exercise the actual camera-height integration, slow-frame wall collision,
    // gravity-transition resets and ignored input after timeout.
    void TestJumpInGame() {
        using namespace MazeGeometry;
        for (GameMode Mode : {GameMode::Normal, GameMode::ZeroGravity}) {
            Game CurrentGame(25, 100, Mode);
            InputState CurrentInput{};
            CurrentInput.Dt = Tick;
            CurrentInput.JumpPressed = true;
            CurrentGame.Update(CurrentInput);
            RequireJump(CurrentGame.Y() > StandingEyeHeight,
                        "Space must raise the actual game camera from the floor");
            CurrentInput.JumpPressed = false;
            for (int Frame = 0; Frame < 100; ++Frame) {
                CurrentGame.Update(CurrentInput);
                RequireJump(CurrentGame.Y() <= CeilingHeight - SurfaceOffset + .001f &&
                    CurrentGame.Y() >= StandingEyeHeight && !CurrentGame.SurfaceAttached(),
                    "floor jumping must not enter ceilings, fixtures or change gravity");
            }
            RequireJump(CurrentGame.Y() == StandingEyeHeight, "the game must land at its original standing height");
        }

        Game CurrentWall(25, 100);
        constexpr float Pi = 3.14159265358979323846f;
        const float CurrentTurn = std::remainder(-Pi / 2 - CurrentWall.Yaw(), 2 * Pi);
        CurrentWall.Update({0, 0, CurrentTurn / .0025f, 0, 0, false});
        CurrentWall.Update({1, 0, 0, 0, .75f, false, true});
        RequireJump(CurrentWall.Row() == 0 && CurrentWall.Z() >= -HalfCell + PlayerRadius - .001f,
            "a jumping slow frame must not tunnel through the maze boundary");
        RequireJump(CurrentWall.Y() == StandingEyeHeight, "a slow frame must integrate the complete jump and landing");

        Game CurrentGravity(25, 100, GameMode::ZeroGravity);
        CurrentGravity.Update({0, 0, 0, 0, Tick, false, true});
        CurrentGravity.Update({0, 0, 0, -400, 0, false});
        CurrentGravity.Update({0, 0, 0, 0, 0, true});
        CurrentGravity.Update({0, 0, 0, 0, .25f, false, true});
        RequireJump(CurrentGravity.SurfaceAttached() && std::fabs(CurrentGravity.Y() - (CeilingHeight - StandingEyeHeight)) < .001f,
            "attaching mid-jump must safely clear floor momentum and height");
        CurrentGravity.Update({0, 0, 0, 0, .1f, false, true});
        RequireJump(std::fabs(CurrentGravity.Y() - (CeilingHeight - StandingEyeHeight)) < .001f,
            "Space must not launch the player out of an attached ceiling");
        CurrentGravity.Update({0, 0, 0, 0, .25f, true});
        CurrentGravity.Update({0, 0, 0, 0, .25f, false});
        RequireJump(!CurrentGravity.SurfaceAttached() && CurrentGravity.Y() == StandingEyeHeight,
            "returning from the ceiling must clear pending jump input");

        Game Finished(10, .01f);
        Finished.Update({0, 0, 0, 0, .02f, false, true});
        RequireJump(Finished.Lost() && Finished.Y() == StandingEyeHeight,
            "finished games must ignore jump input");
    }

    // Simulate jump arcs at 30, 60, and 144 frames per second and compare their apex and final
    // grounded height. This catches integration changes that make jumping strongly dependent on
    // frame rate.
    void TestFrameRates() {
        for (float Rate : {30.0f, 60.0f, 144.0f}) {
            Game CurrentGame(15, 100);
            float Apex = 0;
            for (int Frame = 0; Frame < (int)Rate; ++Frame) {
                CurrentGame.Update({0, 0, 0, 0, 1 / Rate, false, Frame == 0});
                Apex = std::max(Apex, CurrentGame.Y());
            }
            RequireJump(std::fabs(Apex - (MazeGeometry::StandingEyeHeight + .4266667f)) < .006f &&
                CurrentGame.Y() == MazeGeometry::StandingEyeHeight,
                "jump height and landing must remain consistent at 30, 60 and 144 FPS");
        }
    }
}

// Run the standalone JumpMovement cases and their integration through Game from the shared
// gameplay executable. Fail through descriptive exceptions while leaving user input and
// persistent saves untouched.
void RunJumpTests() {
    TestJumpArcAndBuffer();
    TestAirStrafeAndFriction();
    TestSeparateWalkingSpeed();
    TestJumpInGame();
    TestFrameRates();
}
