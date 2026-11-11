#include "Renderer.h"
#include "../data/MazeGeometry.h"
#include "../data/Validation.h"
#include "CorridorStyle.h"
#include "MazeShaders.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

namespace {
static_assert(MaxSize <= MazeShaders::MaxMazeRows);
static_assert(MazeShaders::MaxMazeRows <= 64);
constexpr float ViewDistance = 15.0f;

// Darker wall pigments let the red/cream carpet lead, while separate gilt trim and lamps retain
// their highlights. Balance the honey stone toward warm charcoal rather than bright orange.
constexpr Color WallMaterialTint{180, 180, 180, 255};
constexpr Color MarbleMaterialTint{126, 139, 157, 255};

struct Corner {
    float CurrentX, CurrentY, CurrentZ, U, V;
};
struct Wall {
    float Lx, Lz, Rx, Rz;
    int Row, Col, CurrentSide;
};

// Convert authored decoration coordinates into world space by scaling X/Z with cell width and Y
// with ceiling height. Detail helpers share this mapping with the maze's physical dimensions.
Vector3 World(Vector3 Point) {
    return {Point.x * MazeGeometry::CellSize, Point.y * MazeGeometry::CeilingHeight,
            Point.z * MazeGeometry::CellSize};
}

// Draw a decoration cube after converting its center and dimensions from authored cell
// coordinates into world units. The helper affects visible geometry only and does not add
// collision volumes.
void DetailCube(Vector3 Point, float Width, float Height, float Depth, Color CurrentColor) {
    const Vector3 CurrentCenter = World(Point);
    const float X0 = CurrentCenter.x - Width * MazeGeometry::CellSize * .5f;
    const float X1 = CurrentCenter.x + Width * MazeGeometry::CellSize * .5f;
    const float Y0 = CurrentCenter.y - Height * MazeGeometry::CeilingHeight * .5f;
    const float Y1 = CurrentCenter.y + Height * MazeGeometry::CeilingHeight * .5f;
    const float Z0 = CurrentCenter.z - Depth * MazeGeometry::CellSize * .5f;
    const float Z1 = CurrentCenter.z + Depth * MazeGeometry::CellSize * .5f;
    rlBegin(RL_QUADS);
    rlColor4ub(CurrentColor.r, CurrentColor.g, CurrentColor.b, CurrentColor.a);
    rlTexCoord2f(0, 0);
    for (Vector3 CurrentVertex :
         {Vector3{X0, Y0, Z1}, {X1, Y0, Z1}, {X1, Y1, Z1}, {X0, Y1, Z1}, {X1, Y0, Z0},
          {X0, Y0, Z0},        {X0, Y1, Z0}, {X1, Y1, Z0}, {X0, Y1, Z1}, {X1, Y1, Z1},
          {X1, Y1, Z0},        {X0, Y1, Z0}, {X0, Y0, Z0}, {X1, Y0, Z0}, {X1, Y0, Z1},
          {X0, Y0, Z1},        {X1, Y0, Z1}, {X1, Y0, Z0}, {X1, Y1, Z0}, {X1, Y1, Z1},
          {X0, Y0, Z0},        {X0, Y0, Z1}, {X0, Y1, Z1}, {X0, Y1, Z0}})
        rlVertex3f(CurrentVertex.x, CurrentVertex.y, CurrentVertex.z);
    rlEnd();
}

// Cull shallow architectural relief outside the camera's nearby view, leaving distant ornament to
// the wall texture. Lighting still considers nearby sources in all directions so turning does not
// remove illumination.
bool DetailVisible(const Camera3D& CurrentCamera, Vector3 CurrentPosition) {
    const float Dx = CurrentPosition.x - CurrentCamera.position.x, Dy = CurrentPosition.y - CurrentCamera.position.y,
                Dz = CurrentPosition.z - CurrentCamera.position.z;
    const float Distance2 = Dx * Dx + Dy * Dy + Dz * Dz;
    if (Distance2 > 16.0f * 16.0f)
        return false;
    const Vector3 CurrentForward{CurrentCamera.target.x - CurrentCamera.position.x, CurrentCamera.target.y - CurrentCamera.position.y,
                          CurrentCamera.target.z - CurrentCamera.position.z};
    return Dx * CurrentForward.x + Dy * CurrentForward.y + Dz * CurrentForward.z + 1.8f >= std::sqrt(Distance2) * .45f;
}

// Submit a corner's color, texture coordinates, and world-space position to the active rlgl
// batch. The caller establishes its primitive, texture, and shader state before emitting
// vertices.
void Emit(const Corner& Point, Color CurrentColor) {
    rlColor4ub(CurrentColor.r, CurrentColor.g, CurrentColor.b, CurrentColor.a);
    rlTexCoord2f(Point.U, Point.V);
    Vector3 CurrentPosition = World({Point.CurrentX, Point.CurrentY, Point.CurrentZ});
    rlVertex3f(CurrentPosition.x, CurrentPosition.y, CurrentPosition.z);
}

// Submit four ordered corners with a shared color to the active quad batch. Corner ordering and
// UVs come from the caller so floors, ceilings, and wall faces keep their intended orientation.
void Quad(const Corner& CurrentA, const Corner& B, const Corner& CurrentC, const Corner& CurrentD, Color CurrentColor) {
    Emit(CurrentA, CurrentColor);
    Emit(B, CurrentColor);
    Emit(CurrentC, CurrentColor);
    Emit(CurrentD, CurrentColor);
}

// Map normalized horizontal bounds and authored heights onto an exposed wall face and submit its
// textured strip. Offset the geometry slightly from the wall to avoid depth overlap with the base
// surface.
void WallStrip(const Wall& CurrentWall, float CurrentLeft, float CurrentRight, float Bottom, float Top, Color CurrentColor,
               float Inset = 0.004f) {
    float Dx = CurrentWall.Rx - CurrentWall.Lx, Dz = CurrentWall.Rz - CurrentWall.Lz;
    float X0 = CurrentWall.Lx + Dx * CurrentLeft - Dz * Inset;
    float Z0 = CurrentWall.Lz + Dz * CurrentLeft + Dx * Inset;
    float X1 = CurrentWall.Lx + Dx * CurrentRight - Dz * Inset;
    float Z1 = CurrentWall.Lz + Dz * CurrentRight + Dx * Inset;
    Quad({X0, Bottom, Z0, CurrentLeft, 1 - Bottom}, {X1, Bottom, Z1, CurrentRight, 1 - Bottom},
         {X1, Top, Z1, CurrentRight, 1 - Top}, {X0, Top, Z0, CurrentLeft, 1 - Top}, CurrentColor);
}

// Convert a wall-local across, height, and depth position into world coordinates using the face's
// directed edge. The perpendicular depth offset keeps details consistently oriented on all wall
// sides.
Vector3 WallPoint(const Wall& CurrentWall, float Across, float Height, float Depth) {
    const float Dx = CurrentWall.Rx - CurrentWall.Lx, Dz = CurrentWall.Rz - CurrentWall.Lz;
    return World({CurrentWall.Lx + Dx * Across - Dz * Depth, Height, CurrentWall.Lz + Dz * Across + Dx * Depth});
}

// Draw a shallow architectural block in an exposed wall's local frame. Keep the raised detail
// close to the surface so its visible ornament does not narrow the playable corridor.
void WallBlock(const Wall& CurrentWall, float CurrentLeft, float CurrentRight, float Bottom, float Top, float Depth,
               Color CurrentColor) {
    const float Dx = CurrentWall.Rx - CurrentWall.Lx, Dz = CurrentWall.Rz - CurrentWall.Lz;
    const float Middle = (CurrentLeft + CurrentRight) * .5f;
    const Vector3 CurrentCenter{CurrentWall.Lx + Dx * Middle - Dz * Depth * .5f, (Bottom + Top) * .5f,
                         CurrentWall.Lz + Dz * Middle + Dx * Depth * .5f};
    DetailCube(CurrentCenter, Dx ? CurrentRight - CurrentLeft : Depth, Top - Bottom, Dx ? Depth : CurrentRight - CurrentLeft, CurrentColor);
}

// Map stone texture coordinates over a shallow pilaster's front and returns using stable world
// proportions. Gilded molding covers its caps while narrow faces retain consistent material
// density.
void MarbleBlock(const Wall& CurrentWall, float CurrentLeft, float CurrentRight, float Bottom, float Top, float Depth) {
    const float Dx = CurrentWall.Rx - CurrentWall.Lx, Dz = CurrentWall.Rz - CurrentWall.Lz;
    // Map one pilaster-local corner into its wall frame and attach the requested stone UV
    // coordinate. The enclosing marble detail uses these corners to keep front and return texture
    // density consistent.
    auto Point = [&](float Across, float Height, float CurrentOffset, float U) {
        return Corner{CurrentWall.Lx + Dx * Across - Dz * CurrentOffset, Height,
                      CurrentWall.Lz + Dz * Across + Dx * CurrentOffset, U * MazeGeometry::CellSize / .8f,
                      1 - Height * MazeGeometry::CeilingHeight / .8f};
    };
    Quad(Point(CurrentLeft, Bottom, Depth, CurrentLeft), Point(CurrentRight, Bottom, Depth, CurrentRight),
         Point(CurrentRight, Top, Depth, CurrentRight), Point(CurrentLeft, Top, Depth, CurrentLeft), MarbleMaterialTint);
    Quad(Point(CurrentLeft, Bottom, 0, CurrentLeft - Depth), Point(CurrentLeft, Bottom, Depth, CurrentLeft),
         Point(CurrentLeft, Top, Depth, CurrentLeft), Point(CurrentLeft, Top, 0, CurrentLeft - Depth), MarbleMaterialTint);
    Quad(Point(CurrentRight, Bottom, Depth, CurrentRight), Point(CurrentRight, Bottom, 0, CurrentRight + Depth),
         Point(CurrentRight, Top, 0, CurrentRight + Depth), Point(CurrentRight, Top, Depth, CurrentRight), MarbleMaterialTint);
}

// Draw curved gilt support geometry for the paired upright glass cups. These visual fixture parts
// match the corridor artwork and do not participate in gameplay collision.
void SconceArm(const Wall& CurrentWall, int CurrentSide, Color Metal) {
    // Evaluate a point along the authored curved sconce arm. Reusing this local curve keeps the
    // connected support segments aligned with the fixture's wall frame.
    auto Point = [&](float CurrentT) {
        const float U = 1 - CurrentT;
        const float Across =
            .5f + CurrentSide * (3 * U * U * CurrentT * .09f + 3 * U * CurrentT * CurrentT * .10f + CurrentT * CurrentT * CurrentT * .06f);
        const float Height =
            U * U * U * .727f + 3 * U * U * CurrentT * .665f + 3 * U * CurrentT * CurrentT * .720f + CurrentT * CurrentT * CurrentT * .790f;
        return WallPoint(CurrentWall, Across, Height + (CurrentSide > 0 ? .018f * CurrentT : 0),
                         .028f + .072f * std::sin(CurrentT * PI * .5f));
    };
    Vector3 Previous = Point(0);
    for (int Segment = 1; Segment <= 6; ++Segment) {
        const Vector3 Next = Point(Segment / 6.0f);
        DrawCylinderEx(Previous, Next, .007f, .007f, 6, Metal);
        Previous = Next;
    }
}

// Visit open maze cells inside the renderer's bounded neighborhood and distance cutoff, invoking
// the supplied drawing callback. Limiting work to nearby cells keeps large mazes from submitting
// their full geometry every frame.
template <typename Draw> void NearbyCells(const Game& CurrentGame, Draw CurrentDraw) {
    int CurrentReach = (int)ViewDistance + 1;
    int CurrentSize = CurrentGame.Maze().Size();
    for (int Row = std::max(0, CurrentGame.Row() - CurrentReach); Row <= std::min(CurrentSize - 1, CurrentGame.Row() + CurrentReach);
         ++Row)
        for (int Col = std::max(0, CurrentGame.Col() - CurrentReach);
             Col <= std::min(CurrentSize - 1, CurrentGame.Col() + CurrentReach); ++Col) {
            if (CurrentGame.Maze().Cell(Row, Col) == '#')
                continue;
            float Dx = Col - CurrentGame.X() / MazeGeometry::CellSize;
            float Dz = Row - CurrentGame.Z() / MazeGeometry::CellSize;
            if (Dx * Dx + Dz * Dz > (ViewDistance + 1) * (ViewDistance + 1))
                continue;
            CurrentDraw(Row, Col);
        }
}

// Enumerate passage-cell edges adjacent to walls and pass each consistently oriented face to the
// supplied callback. Shared face coordinates keep wallpaper, trim, and light placement aligned.
template <typename Draw> void ExposedWalls(const Game& CurrentGame, Draw CurrentDraw) {
    // Enumerate solid neighbors of this visible passage and provide consistently directed wall
    // faces to the caller. The outer nearby-cell iterator has already limited rendering distance.
    NearbyCells(CurrentGame, [&](int Row, int Col) {
        float CurrentX = (float)Col, CurrentZ = (float)Row;
        if (CurrentGame.Maze().Cell(Row - 1, Col) == '#')
            CurrentDraw(Wall{CurrentX - .5f, CurrentZ - .5f, CurrentX + .5f, CurrentZ - .5f, Row, Col, 0});
        if (CurrentGame.Maze().Cell(Row + 1, Col) == '#')
            CurrentDraw(Wall{CurrentX + .5f, CurrentZ + .5f, CurrentX - .5f, CurrentZ + .5f, Row, Col, 1});
        if (CurrentGame.Maze().Cell(Row, Col - 1) == '#')
            CurrentDraw(Wall{CurrentX - .5f, CurrentZ + .5f, CurrentX - .5f, CurrentZ - .5f, Row, Col, 2});
        if (CurrentGame.Maze().Cell(Row, Col + 1) == '#')
            CurrentDraw(Wall{CurrentX + .5f, CurrentZ - .5f, CurrentX + .5f, CurrentZ + .5f, Row, Col, 3});
    });
}

// Submit a horizontal decorative quad around an authored center with the requested half-width,
// half-depth, height, and UV region. Convert its points to world units before adding them to the
// batch.
void FloorPatch(float CurrentX, float CurrentZ, float CurrentY, float HalfWidth, float HalfDepth, Color CurrentColor) {
    Quad({CurrentX - HalfWidth, CurrentY, CurrentZ - HalfDepth, 0, 0}, {CurrentX - HalfWidth, CurrentY, CurrentZ + HalfDepth, 0, 1},
         {CurrentX + HalfWidth, CurrentY, CurrentZ + HalfDepth, 1, 1}, {CurrentX + HalfWidth, CurrentY, CurrentZ - HalfDepth, 1, 0}, CurrentColor);
}

// Join two woven border runs around an outside wall corner with matching miters. This completes
// the continuous carpet runner without laying a transverse stripe at every cell boundary.
void CarpetCorner(float CurrentX, float CurrentZ, float InwardX, float InwardZ) {
    using namespace CarpetLayout;
    const Corner CurrentCorner{CurrentX, 0, CurrentZ, BorderStartU, 0};
    const Corner AlongX{CurrentX + InwardX * BorderWidth, 0, CurrentZ, BorderU, 0};
    const Corner Inside{CurrentX + InwardX * BorderWidth, 0, CurrentZ + InwardZ * BorderWidth, BorderU,
                        BorderWidth};
    const Corner AlongZ{CurrentX, 0, CurrentZ + InwardZ * BorderWidth, BorderU, 1};
    const Corner OppositeCorner{CurrentX, 0, CurrentZ, BorderStartU, 1};
    const Corner OppositeInside{Inside.CurrentX, 0, Inside.CurrentZ, BorderU, 1 - BorderWidth};
    if (InwardX * InwardZ < 0) {
        Quad(CurrentCorner, AlongX, Inside, Inside, WHITE);
        Quad(OppositeCorner, OppositeInside, AlongZ, AlongZ, WHITE);
    } else {
        Quad(CurrentCorner, Inside, AlongX, AlongX, WHITE);
        Quad(OppositeCorner, AlongZ, OppositeInside, OppositeInside, WHITE);
    }
}

// Crop the carpet's center field to the inner edge of neighboring wall borders. Adjacent field
// patches share edges without overlap, preventing coplanar flicker and redundant floor shading.
void CarpetField(const Game& CurrentGame, int Row, int Col) {
    using namespace CarpetLayout;
    const float CurrentLeft = CurrentGame.Maze().Cell(Row, Col - 1) == '#' ? BorderWidth : 0;
    const float CurrentRight = CurrentGame.Maze().Cell(Row, Col + 1) == '#' ? BorderWidth : 0;
    const float Top = CurrentGame.Maze().Cell(Row - 1, Col) == '#' ? BorderWidth : 0;
    const float Bottom = CurrentGame.Maze().Cell(Row + 1, Col) == '#' ? BorderWidth : 0;
    const bool Nw = !CurrentLeft && !Top && CurrentGame.Maze().Cell(Row - 1, Col - 1) == '#';
    const bool Ne = !CurrentRight && !Top && CurrentGame.Maze().Cell(Row - 1, Col + 1) == '#';
    const bool Sw = !CurrentLeft && !Bottom && CurrentGame.Maze().Cell(Row + 1, Col - 1) == '#';
    const bool Se = !CurrentRight && !Bottom && CurrentGame.Maze().Cell(Row + 1, Col + 1) == '#';
    // Submit one carpet-border region with wall-local UV bounds and matching geometry. Splitting
    // the runner into these patches lets corner miters share exact edges.
    auto Patch = [&](float U0, float V0, float U1, float V1) {
        Quad({Col - .5f + U0, 0, Row - .5f + V0, U0 * FieldU, V0},
             {Col - .5f + U0, 0, Row - .5f + V1, U0 * FieldU, V1},
             {Col - .5f + U1, 0, Row - .5f + V1, U1 * FieldU, V1},
             {Col - .5f + U1, 0, Row - .5f + V0, U1 * FieldU, V0}, WHITE);
    };
    if (!Nw && !Ne && !Sw && !Se) {
        Patch(CurrentLeft, Top, 1 - CurrentRight, 1 - Bottom);
        return;
    }
    const float Edges[] = {0, BorderWidth, 1 - BorderWidth, 1};
    for (int CurrentY = 0; CurrentY < 3; ++CurrentY)
        for (int CurrentX = 0; CurrentX < 3; ++CurrentX) {
            if ((CurrentX == 0 && CurrentLeft) || (CurrentX == 2 && CurrentRight) || (CurrentY == 0 && Top) || (CurrentY == 2 && Bottom))
                continue;
            if ((CurrentX == 0 && CurrentY == 0 && Nw) || (CurrentX == 2 && CurrentY == 0 && Ne) || (CurrentX == 0 && CurrentY == 2 && Sw) ||
                (CurrentX == 2 && CurrentY == 2 && Se))
                continue;
            Patch(Edges[CurrentX], Edges[CurrentY], Edges[CurrentX + 1], Edges[CurrentY + 1]);
        }
    if (Nw)
        CarpetCorner(Col - .5f, Row - .5f, 1, 1);
    if (Ne)
        CarpetCorner(Col + .5f, Row - .5f, -1, 1);
    if (Sw)
        CarpetCorner(Col - .5f, Row + .5f, 1, -1);
    if (Se)
        CarpetCorner(Col + .5f, Row + .5f, -1, -1);
}

// Place the woven border beside exposed maze walls and trim its ends into shared corner miters.
// Avoid borders at open cell boundaries so straight passages read as one continuous runner.
void CarpetBorder(const Game& CurrentGame, const Wall& CurrentWall) {
    using namespace CarpetLayout;
    const float Dx = CurrentWall.Rx - CurrentWall.Lx, Dz = CurrentWall.Rz - CurrentWall.Lz;
    const bool LeftCorner = CurrentGame.Maze().Cell(CurrentWall.Row - (int)Dz, CurrentWall.Col - (int)Dx) == '#';
    const bool RightCorner = CurrentGame.Maze().Cell(CurrentWall.Row + (int)Dz, CurrentWall.Col + (int)Dx) == '#';
    const float CurrentStart = LeftCorner ? BorderWidth : 0, End = RightCorner ? BorderWidth : 0;
    const float InnerAx = CurrentWall.Lx - Dz * BorderWidth + Dx * CurrentStart;
    const float InnerAz = CurrentWall.Lz + Dx * BorderWidth + Dz * CurrentStart;
    const float InnerBx = CurrentWall.Rx - Dz * BorderWidth - Dx * End;
    const float InnerBz = CurrentWall.Rz + Dx * BorderWidth - Dz * End;
    constexpr float CurrentY = 0;
    Quad({CurrentWall.Lx, CurrentY, CurrentWall.Lz, BorderStartU, 0}, {InnerAx, CurrentY, InnerAz, BorderU, CurrentStart},
         {InnerBx, CurrentY, InnerBz, BorderU, 1 - End}, {CurrentWall.Rx, CurrentY, CurrentWall.Rz, BorderStartU, 1}, WHITE);
}

// Choose ceiling-lamp cells deterministically from their coordinates and nearby passage
// connectivity. The same layout predicate supports visible fixtures and the light list without
// consuming random state.
bool CeilingFixture(const Game& CurrentGame, int Row, int Col) {
    int Neighbours =
        (CurrentGame.Maze().Cell(Row - 1, Col) != '#') + (CurrentGame.Maze().Cell(Row + 1, Col) != '#') +
        (CurrentGame.Maze().Cell(Row, Col - 1) != '#') + (CurrentGame.Maze().Cell(Row, Col + 1) != '#');
    return (Row + Col) % 3 == 0 || Neighbours >= 3;
}

// Choose exposed wall faces for sconces with a repeatable row, column, and side pattern. Keeping
// this predicate deterministic aligns visible lamp geometry with uploaded light sources.
bool WallFixture(const Wall& CurrentWall) {
    return (CurrentWall.Row * 13 + CurrentWall.Col * 7 + CurrentWall.CurrentSide * 3) % 5 <= 1;
}

}

// Initialize the perspective camera and custom lighting shader, caching its uniform locations
// when compilation succeeds. Keep valid fallback state when a custom shader is unavailable;
// material preparation is coordinated separately.
Renderer::Renderer() {
    Cam = Camera3D{};
    Cam.up = {0, 1, 0};
    Cam.fovy = 70;
    Cam.projection = CAMERA_PERSPECTIVE;
    Atmosphere = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    if (Atmosphere.id != 0 && Atmosphere.id != rlGetShaderIdDefault()) {
        EyeLocation = GetShaderLocation(Atmosphere, "EyePosition");
        DirectionLocation = GetShaderLocation(Atmosphere, "ViewDirection");
        EmissionLocation = GetShaderLocation(Atmosphere, "Emission");
        MaterialLocation = GetShaderLocation(Atmosphere, "MaterialParams");
        WallSurfaceLocation = GetShaderLocation(Atmosphere, "WallSurface");
        HdrLocation = GetShaderLocation(Atmosphere, "HdrOutput");
        LightPositionsLocation = GetShaderLocation(Atmosphere, "LightPositions");
        LightDirectionsLocation = GetShaderLocation(Atmosphere, "LightDirections");
        LightExtentsLocation = GetShaderLocation(Atmosphere, "LightExtents");
        LightCountLocation = GetShaderLocation(Atmosphere, "LightCount");
        WallRowsLocation = GetShaderLocation(Atmosphere, "WallRows");
        MazeSizeLocation = GetShaderLocation(Atmosphere, "MazeSize");
        CellSizeLocation = GetShaderLocation(Atmosphere, "CellSize");
        FixtureColorLocation = GetShaderLocation(Atmosphere, "FixtureColor");
        FogColorLocation = GetShaderLocation(Atmosphere, "FogColor");
        BounceColorLocation = GetShaderLocation(Atmosphere, "BounceColor");
        RoomHeightLocation = GetShaderLocation(Atmosphere, "RoomHeight");
    } else {
        Atmosphere = {};
    }
}

// Release the renderer's owned lighting shader while member resource owners clean up their
// textures and post-processing targets. Application destroys this object before closing the
// graphics context.
Renderer::~Renderer() {
    if (Atmosphere.id)
        UnloadShader(Atmosphere);
}

// Reuse already prepared materials when the requested theme is unchanged. Build a complete
// replacement first and publish it only when valid, preserving the old resources if material
// creation fails.
void Renderer::PrepareMazeTheme(MazeTheme Theme) {
    if (TexturesReady && ActiveTheme == Theme)
        return;
    rlDrawRenderBatchActive();
    const MazePalette& CurrentChosen = GetMazePalette(Theme);
    Textures Replacement = CreateTextures(CurrentChosen);
    if (!Replacement.Valid())
        return;
    Tex = std::move(Replacement);
    CurrentPalette = CurrentChosen;
    ActiveTheme = Theme;
    TexturesReady = true;
}

// Render the world first, then draw the gameplay HUD in screen space with optional map-peek
// state. Paused and result views can use drawWorld when gameplay overlays should be omitted.
void Renderer::Draw(const Game& CurrentGame, const MapPeek* CurrentMapPeek) {
    DrawWorld(CurrentGame);
    Hud.Draw(CurrentGame, CurrentPalette, CurrentMapPeek);
}

// Build the camera from the game's displayed position, direction, and up vector, then draw lit
// maze surfaces, details, hazards, and exit. Use post-processing when available and keep the
// fallback world render functional.
void Renderer::DrawWorld(const Game& CurrentGame) {
    PrepareMazeTheme(CurrentGame.Theme());
    Cam.position = CurrentGame.Position();
    const Vector3 Direction = CurrentGame.CameraDirection();
    Cam.target = {Cam.position.x + Direction.x, Cam.position.y + Direction.y,
                  Cam.position.z + Direction.z};
    Cam.up = CurrentGame.CameraUp();

    const bool Glow = Atmosphere.id && CurrentBloom.Begin(true);
    ClearBackground(CurrentPalette.Fog);
    BeginMode3D(Cam);
    if (Glow)
        CurrentBloom.SetView(rlGetMatrixModelview(), rlGetMatrixProjection());
    if (Atmosphere.id) {
        float Emission = 0;
        const float Hdr = Glow ? 1.0f : 0.0f;
        const float CurrentFixture[] = {CurrentPalette.Lamp.r / 255.0f, CurrentPalette.Lamp.g / 255.0f,
                                 CurrentPalette.Lamp.b / 255.0f};
        const float Fog[] = {CurrentPalette.Fog.r / 255.0f, CurrentPalette.Fog.g / 255.0f,
                             CurrentPalette.Fog.b / 255.0f};
        const Color BounceColor = ColorLerp(CurrentPalette.CurrentWall, CurrentPalette.CurrentPaper, .35f);
        const float Bounce[] = {BounceColor.r / 255.0f, BounceColor.g / 255.0f,
                                BounceColor.b / 255.0f};
        const float RoomHeight = MazeGeometry::CeilingHeight;
        SetShaderValue(Atmosphere, FixtureColorLocation, CurrentFixture, SHADER_UNIFORM_VEC3);
        SetShaderValue(Atmosphere, FogColorLocation, Fog, SHADER_UNIFORM_VEC3);
        SetShaderValue(Atmosphere, BounceColorLocation, Bounce, SHADER_UNIFORM_VEC3);
        SetShaderValue(Atmosphere, RoomHeightLocation, &RoomHeight, SHADER_UNIFORM_FLOAT);
        SetShaderValue(Atmosphere, EyeLocation, &Cam.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(Atmosphere, DirectionLocation, &Direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(Atmosphere, EmissionLocation, &Emission, SHADER_UNIFORM_FLOAT);
        SetShaderValue(Atmosphere, HdrLocation, &Hdr, SHADER_UNIFORM_FLOAT);
        UpdateLights(CurrentGame);
        BeginShaderMode(Atmosphere);
    }
    DrawMaze(CurrentGame);
    DrawDetails(CurrentGame);
    GetMaterial(.38f, .08f, .0002f, .20f);
    DrawExit(CurrentGame, false);
    GetMaterial(.33f, .55f, 0, .10f);
    DrawObstacles(CurrentGame, false);
    // Submit queued surface geometry before enabling emission so it retains the lighting uniforms
    // used when it was queued.
    rlDrawRenderBatchActive();
    if (Atmosphere.id) {
        float Emission = 1;
        SetShaderValue(Atmosphere, EmissionLocation, &Emission, SHADER_UNIFORM_FLOAT);
    }
    DrawLights(CurrentGame);
    DrawExit(CurrentGame, true);
    DrawObstacles(CurrentGame, true);
    if (Atmosphere.id)
        EndShaderMode();
    EndMode3D();
    if (Glow)
        CurrentBloom.End();

    const int Width = GetScreenWidth(), Height = GetScreenHeight();
    DrawRectangleGradientV(0, 0, Width, UiSize(160), Fade(CurrentPalette.Fog, .45f), Fade(CurrentPalette.Fog, 0));
    DrawRectangleGradientV(0, Height - UiSize(130), Width, UiSize(130), Fade(CurrentPalette.Fog, 0),
                           Fade(CurrentPalette.Fog, .63f));
}

// Flush queued vertices before changing the material uniforms for following geometry. Roughness,
// metalness, relief, clear coat, and wall-surface flags must not retroactively affect a previous
// batch.
void Renderer::GetMaterial(float CurrentRoughness, float Metalness, float Relief, float ClearCoat,
                        bool WallSurface) {
    if (!Atmosphere.id)
        return;
    rlDrawRenderBatchActive();
    const float Values[] = {CurrentRoughness, Metalness, Relief, ClearCoat};
    const int CurrentWall = WallSurface ? 1 : 0;
    SetShaderValue(Atmosphere, MaterialLocation, Values, SHADER_UNIFORM_VEC4);
    SetShaderValue(Atmosphere, WallSurfaceLocation, &CurrentWall, SHADER_UNIFORM_INT);
}

// Batch nearby floor, ceiling, and exposed wall surfaces using their textures, followed by
// shallow architectural pieces. Manage texture and culling state explicitly so each surface uses
// the intended material.
void Renderer::DrawMaze(const Game& CurrentGame) {
    rlDisableBackfaceCulling();
    GetMaterial(.84f, 0, .0012f, 0, true);
    rlSetTexture(Tex.GetWall().id);
    rlBegin(RL_QUADS);
    // Draw the base wallpaper strip for each exposed wall face using the active wall texture.
    // Geometry follows the shared face coordinates before later relief and trim passes.
    ExposedWalls(CurrentGame, [](const Wall& CurrentWall) {
        unsigned char Brightness = CurrentWall.CurrentSide < 2 ? 255 : 235;
        WallStrip(CurrentWall, 0, 1, 0, 1,
                  ColorTint(WallMaterialTint, {Brightness, Brightness, Brightness, 255}), 0);
    });
    rlEnd();

    GetMaterial(.96f, 0, .0008f);
    rlSetTexture(Tex.Floor().id);
    rlBegin(RL_QUADS);
    // Draw the carpet's center field for every nearby open cell. Each patch is cropped against
    // neighboring borders so adjacent surfaces do not overlap.
    NearbyCells(CurrentGame, [&](int Row, int Col) { CarpetField(CurrentGame, Row, Col); });
    // Draw a woven carpet border along each exposed wall. Open passage boundaries receive no
    // transverse stripe, keeping the floor runner visually continuous.
    ExposedWalls(CurrentGame, [&](const Wall& CurrentWall) { CarpetBorder(CurrentGame, CurrentWall); });
    rlEnd();

    GetMaterial(.72f, 0, .00025f, .02f);
    rlSetTexture(Tex.Ceiling().id);
    rlBegin(RL_QUADS);
    NearbyCells(CurrentGame,
                // Draw one ceiling patch for each nearby passage cell using the active ceiling
                // material. Authored height one maps to MazeGeometry::CeilingHeight.
                [](int Row, int Col) { FloorPatch((float)Col, (float)Row, 1, .5f, .5f, WHITE); });
    rlEnd();

    rlSetTexture(rlGetTextureIdDefault());
    // Submit the two-sided geometry before restoring backface culling; queued vertices do not
    // retain the previous culling state.
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// Upload wall occupancy masks when the run layout changes, then select a bounded list of nearby
// ceiling and wall sources. Cache layout identity by size and seed so per-frame lighting cost
// stays bounded.
void Renderer::UpdateLights(const Game& CurrentGame) {
    using namespace MazeShaders;
    const int CurrentSize = CurrentGame.Maze().Size();
    if (CurrentSize != LightMazeSize || CurrentGame.Seed() != LightMazeSeed) {
        int Rows[MaxMazeRows * 2]{};
        for (int Row = 0; Row < std::min(CurrentSize, MaxMazeRows); ++Row)
            for (int Col = 0; Col < std::min(CurrentSize, 64); ++Col)
                if (CurrentGame.Maze().Cell(Row, Col) == '#')
                    Rows[Row * 2 + Col / 32] |= std::bit_cast<int>(std::uint32_t{1} << (Col % 32));
        SetShaderValueV(Atmosphere, WallRowsLocation, Rows, SHADER_UNIFORM_IVEC2, MaxMazeRows);
        SetShaderValue(Atmosphere, MazeSizeLocation, &CurrentSize, SHADER_UNIFORM_INT);
        const float CurrentCellSize = MazeGeometry::CellSize;
        SetShaderValue(Atmosphere, CellSizeLocation, &CurrentCellSize, SHADER_UNIFORM_FLOAT);
        LightMazeSize = CurrentSize;
        LightMazeSeed = CurrentGame.Seed();
    }
    Vector4 Positions[MaxLights]{}, Directions[MaxLights]{}, Extents[MaxLights]{};
    float Distances[MaxLights]{};
    int CurrentCount = 0;
    // Insert a nearby light into the bounded source list, replacing a farther entry when capacity
    // is full. Keep position, direction, extent, and distance data synchronized for shader
    // upload.
    auto CurrentAdd = [&](Vector3 CurrentPosition, Vector3 Facing, Vector3 HalfAxis, float HalfWidth,
                   float Radius, float Strength) {
        const float Dx = CurrentPosition.x - Cam.position.x, Dy = CurrentPosition.y - Cam.position.y,
                    Dz = CurrentPosition.z - Cam.position.z;
        const float Distance = Dx * Dx + Dy * Dy + Dz * Dz;
        if (Distance > 49.0f)
            return;
        const float Rank = Distance * (Facing.y < -.5f ? .75f : 1.0f);
        if (CurrentCount == MaxLights && Rank >= Distances[MaxLights - 1])
            return;
        int Index = std::min(CurrentCount, MaxLights - 1);
        while (Index > 0 && Rank < Distances[Index - 1]) {
            Positions[Index] = Positions[Index - 1];
            Directions[Index] = Directions[Index - 1];
            Extents[Index] = Extents[Index - 1];
            Distances[Index] = Distances[Index - 1];
            --Index;
        }
        Positions[Index] = {CurrentPosition.x, CurrentPosition.y, CurrentPosition.z, Radius};
        Directions[Index] = {Facing.x, Facing.y, Facing.z, Strength};
        Extents[Index] = {HalfAxis.x, HalfAxis.y, HalfAxis.z, HalfWidth};
        Distances[Index] = Rank;
        CurrentCount = std::min(CurrentCount + 1, MaxLights);
    };
    // Add a ceiling source for an eligible nearby fixture cell using its shared placement rule.
    // Uploaded source geometry must agree with the visible panel drawn later.
    NearbyCells(CurrentGame, [&](int Row, int Col) {
        if (!CeilingFixture(CurrentGame, Row, Col))
            return;
        // A square area approximates the round frosted bowl without changing the light budget.
        CurrentAdd(World({(float)Col, .932f, (float)Row}), {0, -1, 0},
            {.12f * MazeGeometry::CellSize, 0, 0}, .11f * MazeGeometry::CellSize, 3.8f, 1.85f);
    });
    // Add wall-lamp sources for exposed faces selected by wallFixture. Their positions and finite
    // emitter extents match the corresponding sconce geometry.
    ExposedWalls(CurrentGame, [&](const Wall& CurrentWall) {
        if (!WallFixture(CurrentWall))
            return;
        const float Dx = CurrentWall.Rx - CurrentWall.Lx, Dz = CurrentWall.Rz - CurrentWall.Lz;
        Vector3 CurrentCenter = World({(CurrentWall.Lx + CurrentWall.Rx) * .5f, .820f, (CurrentWall.Lz + CurrentWall.Rz) * .5f});
        CurrentCenter.x -= Dz * .23f;
        CurrentCenter.z += Dx * .23f;
        CurrentAdd(CurrentCenter, {-Dz, 0, Dx},
            {Dx * .07f * MazeGeometry::CellSize, 0, Dz * .07f * MazeGeometry::CellSize},
            .035f * MazeGeometry::CellSize, 2.8f, 1.5f);
    });
    SetShaderValue(Atmosphere, LightCountLocation, &CurrentCount, SHADER_UNIFORM_INT);
    if (CurrentCount) {
        SetShaderValueV(Atmosphere, LightPositionsLocation, Positions, SHADER_UNIFORM_VEC4, CurrentCount);
        SetShaderValueV(Atmosphere, LightDirectionsLocation, Directions, SHADER_UNIFORM_VEC4,
                        CurrentCount);
        SetShaderValueV(Atmosphere, LightExtentsLocation, Extents, SHADER_UNIFORM_VEC4, CurrentCount);
    }
}

// Layer skirting, cornices, chair rails, stone pieces, and panel frames over the base wall
// material. Nearby visibility limits the fine geometry while distant texture detail preserves the
// room's appearance.
void Renderer::DrawDetails(const Game& CurrentGame) {
    const CorridorStyle Style = GetCorridorStyle(CurrentPalette);
    rlDisableBackfaceCulling();
    GetMaterial(.27f, 0, .00015f, .40f);
    rlSetTexture(Tex.Marble().id);
    rlBegin(RL_QUADS);
    // Draw the stone architectural pass for nearby exposed faces using the current material
    // state. This separates pilaster surfaces from wallpaper and later gilded trim.
    ExposedWalls(CurrentGame, [&](const Wall& CurrentWall) {
        if (!DetailVisible(Cam, World({(CurrentWall.Lx + CurrentWall.Rx) * .5f, .5f, (CurrentWall.Lz + CurrentWall.Rz) * .5f})))
            return;
        for (float CurrentLeft : {0.0f, .925f}) {
            MarbleBlock(CurrentWall, CurrentLeft, CurrentLeft + .075f, .060f, .925f, .033f);
            MarbleBlock(CurrentWall, CurrentLeft, CurrentLeft + .075f, .065f, .280f, .045f);
        }
    });
    rlEnd();
    GetMaterial(.28f, .78f, 0, .05f);
    rlSetTexture(rlGetTextureIdDefault());
    // Draw the fine frame and molding pass in each exposed wall's local coordinates. Shared face
    // bounds keep the shallow relief aligned with the underlying material.
    ExposedWalls(CurrentGame, [&](const Wall& CurrentWall) {
        if (!DetailVisible(Cam, World({(CurrentWall.Lx + CurrentWall.Rx) * .5f, .5f, (CurrentWall.Lz + CurrentWall.Rz) * .5f})))
            return;
        WallBlock(CurrentWall, 0, 1, .008f, .046f, .018f, Style.TrimShade);
        WallBlock(CurrentWall, 0, 1, .046f, .067f, .025f, Style.Trim);
        WallBlock(CurrentWall, 0, 1, .067f, .080f, .029f, Style.TrimLight);
        WallBlock(CurrentWall, 0, 1, .300f, .312f, .009f, Style.TrimShade);
        WallBlock(CurrentWall, 0, 1, .312f, .325f, .015f, Style.TrimLight);
        WallBlock(CurrentWall, 0, 1, .325f, .330f, .011f, Style.Brass);
        WallBlock(CurrentWall, 0, 1, .916f, .940f, .014f, Style.TrimShade);
        WallBlock(CurrentWall, 0, 1, .940f, .963f, .026f, Style.Trim);
        WallBlock(CurrentWall, 0, 1, .963f, .985f, .038f, Style.TrimLight);
        WallBlock(CurrentWall, 0, 1, .985f, 1.0f, .042f, Style.Trim);
        // Keep the authored beading and carved scroll corners visible on the wall panels.
        // Stepped bases and capitals give the marble columns the loading screen's silhouette.
        for (float CurrentLeft : {0.0f, .925f}) {
            const float CurrentRight = CurrentLeft + .075f;
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .022f, .040f, .056f, Style.TrimShade);
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .040f, .060f, .060f, Style.Trim);
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .060f, .069f, .052f, Style.TrimLight);
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .275f, .290f, .054f, Style.TrimLight);
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .290f, .310f, .064f, Style.Trim);
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .310f, .322f, .055f, Style.TrimLight);
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .915f, .930f, .046f, Style.TrimLight);
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .930f, .954f, .058f, Style.Trim);
            WallBlock(CurrentWall, CurrentLeft, CurrentRight, .954f, .963f, .065f, Style.TrimLight);
            WallBlock(CurrentWall, CurrentLeft + .006f, CurrentLeft + .010f, .322f, .915f, .036f, Style.Trim);
            WallBlock(CurrentWall, CurrentRight - .010f, CurrentRight - .006f, .322f, .915f, .036f, Style.Trim);
        }
        if (WallFixture(CurrentWall)) {
            WallBlock(CurrentWall, .484f, .516f, .670f, .840f, .018f, Style.TrimShade);
            WallBlock(CurrentWall, .489f, .511f, .680f, .830f, .024f, Style.Brass);
            DrawSphereEx(WallPoint(CurrentWall, .5f, .695f, .037f), .018f, 4, 8, Style.Brass);
            for (int CurrentSide : {-1, 1}) {
                SconceArm(CurrentWall, CurrentSide, Style.Brass);
                const float CurrentX = .5f + CurrentSide * .06f, CurrentY = .790f + (CurrentSide > 0 ? .018f : 0);
                DrawCylinderEx(WallPoint(CurrentWall, CurrentX, CurrentY - .006f, .100f),
                               WallPoint(CurrentWall, CurrentX, CurrentY + .004f, .100f), .023f * MazeGeometry::CellSize,
                               .019f * MazeGeometry::CellSize, 12, Style.Brass);
            }
        }
    });
    // Draw ceiling details for nearby passage cells after the wall ornament passes. Use shared
    // geometry scaling so coffer pieces follow the room's actual height.
    NearbyCells(CurrentGame, [&](int Row, int Col) {
        const float CurrentX = (float)Col, CurrentZ = (float)Row;
        if (!DetailVisible(Cam, World({CurrentX, .96f, CurrentZ})))
            return;
        DetailCube({CurrentX - .487f, .987f, CurrentZ}, .026f, .026f, 1.0f, Style.Trim);
        DetailCube({CurrentX, .987f, CurrentZ - .487f}, 1.0f, .026f, .026f, Style.Trim);
        if (CeilingFixture(CurrentGame, Row, Col)) {
            DrawCylinder(World({CurrentX, .989f, CurrentZ}), .19f * MazeGeometry::CellSize,
                         .19f * MazeGeometry::CellSize, .012f, 20, Style.Ceiling);
            DrawCylinder(World({CurrentX, .977f, CurrentZ}), .14f * MazeGeometry::CellSize,
                         .14f * MazeGeometry::CellSize, .014f, 20, Style.TrimShade);
            DrawCylinder(World({CurrentX, .966f, CurrentZ}), .145f * MazeGeometry::CellSize,
                         .145f * MazeGeometry::CellSize, .012f, 20, Style.Brass);
        }
    });
    rlSetTexture(rlGetTextureIdDefault());
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// Draw the paired opal-glass sconce cups and other fixture geometry with their emissive
// treatment. Placement stays clear of ceiling-walking eye positions and never changes gameplay
// collision.
void Renderer::DrawLights(const Game& CurrentGame) {
    // Draw paired wall-sconce cups on faces accepted by wallFixture. These visible fixtures use
    // the same deterministic placement as their lighting sources.
    ExposedWalls(CurrentGame, [&](const Wall& CurrentWall) {
        if (!WallFixture(CurrentWall))
            return;
        if (!DetailVisible(Cam, World({(CurrentWall.Lx + CurrentWall.Rx) * .5f, .8f, (CurrentWall.Lz + CurrentWall.Rz) * .5f})))
            return;
        const Color Glass = ColorLerp(CurrentPalette.Lamp, CurrentPalette.CurrentPaper, .14f);
        for (int CurrentSide : {-1, 1}) {
            const float CurrentX = .5f + CurrentSide * .06f, CurrentY = .790f + (CurrentSide > 0 ? .018f : 0);
            DrawCylinderEx(WallPoint(CurrentWall, CurrentX, CurrentY + .004f, .100f),
                           WallPoint(CurrentWall, CurrentX, CurrentY + .035f, .100f), .017f * MazeGeometry::CellSize,
                           .034f * MazeGeometry::CellSize, 12, Glass);
            DrawCylinderEx(WallPoint(CurrentWall, CurrentX, CurrentY + .035f, .100f),
                           WallPoint(CurrentWall, CurrentX, CurrentY + .068f, .100f), .034f * MazeGeometry::CellSize,
                           .043f * MazeGeometry::CellSize, 12,
                           ColorLerp(Glass, CurrentPalette.CurrentPaper, .18f));
        }
    });
    // Draw ceiling fixtures only in cells accepted by ceilingFixture. This keeps the visible
    // practical lights consistent with the bounded light list.
    NearbyCells(CurrentGame, [&](int Row, int Col) {
        if (!CeilingFixture(CurrentGame, Row, Col))
            return;
        const float CurrentX = (float)Col, CurrentZ = (float)Row;
        if (!DetailVisible(Cam, World({CurrentX, .96f, CurrentZ})))
            return;
        DrawCylinder(World({CurrentX, .953f, CurrentZ}), .132f * MazeGeometry::CellSize,
                     .108f * MazeGeometry::CellSize, .023f, 20,
                     ColorLerp(CurrentPalette.Lamp, CurrentPalette.CurrentPaper, .25f));
        DrawCylinder(World({CurrentX, .939f, CurrentZ}), .108f * MazeGeometry::CellSize,
                     .059f * MazeGeometry::CellSize, .015f, 20, CurrentPalette.Lamp);
        DrawCylinder(World({CurrentX, .932f, CurrentZ}), .059f * MazeGeometry::CellSize,
                     .045f * MazeGeometry::CellSize, .006f, 20,
                     ColorLerp(CurrentPalette.Lamp, CurrentPalette.CurrentPaper, .38f));
    });
}
