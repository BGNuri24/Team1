#include "Renderer.h"
#include <cmath>

namespace {
// Position a conventional doorway against the exit cell's back wall, facing its approach.
struct ExitSpace {
    int Last;
    bool North;

    // Convert exit-local across, height, and depth coordinates into world space according to the
    // final passage's orientation. This lets one authored doorway fit either approach direction.
    Vector3 Point(float Across, float Height, float Depth) const {
        return {(Last + (North ? -Across : .34f + Depth)) * MazeGeometry::CellSize,
                Height * MazeGeometry::CeilingHeight,
                (Last + (North ? .34f + Depth : Across)) * MazeGeometry::CellSize};
    }

    // Draw an exit detail box using its local frame, swapping width and depth for the alternate
    // wall orientation. Scale horizontal dimensions by CellSize and height by CeilingHeight.
    void Box(float Across, float Height, float Depth, float Width, float Tall, float Thick,
             Color CurrentColor) const {
        DrawCube(Point(Across, Height, Depth), (North ? Width : Thick) * MazeGeometry::CellSize,
                 Tall * MazeGeometry::CeilingHeight,
                 (North ? Thick : Width) * MazeGeometry::CellSize, CurrentColor);
    }
};
}

// Draw the final corridor's exit architecture and its lit details using the run's theme and
// geometry. The glow flag selects its emissive contribution while the logical exit remains a maze
// cell.
void Renderer::DrawExit(const Game& CurrentGame, bool Glow) {
    const int Last = CurrentGame.Maze().Size() - 1;
    const float Dx = Last - CurrentGame.X() / MazeGeometry::CellSize;
    const float Dz = Last - CurrentGame.Z() / MazeGeometry::CellSize;
    if (Dx * Dx + Dz * Dz > 15.0f * 15.0f)
        return;
    const bool NorthOpen = CurrentGame.Maze().Cell(Last - 1, Last) != '#';
    const bool WestOpen = CurrentGame.Maze().Cell(Last, Last - 1) != '#';
    const ExitSpace Door{Last, NorthOpen && (!WestOpen || std::fabs(Dz) >= std::fabs(Dx))};
    const Color Metal = ColorLerp(CurrentPalette.Trim, CurrentPalette.CurrentFloor, .20f);
    const Color Leaf = ColorLerp(CurrentPalette.Trim, CurrentPalette.CurrentWall, .55f);
    const Color Hardware = ColorLerp(CurrentPalette.CurrentAccent, CurrentPalette.CurrentPaper, .12f);
    const Color Lamp = ColorLerp(CurrentPalette.CurrentWall, CurrentPalette.Lamp, .70f);

    if (!Glow) {
        // Keep the familiar rectangular silhouette and trim proportions used elsewhere in the hall.
        for (float CurrentSide : {-1.0f, 1.0f}) {
            Door.Box(CurrentSide * .362f, .455f, 0, .065f, .90f, .10f, Metal);
            Door.Box(CurrentSide * .391f, .455f, -.052f, .008f, .90f, .010f, Hardware);
        }
        Door.Box(0, .918f, 0, .79f, .078f, .10f, Metal);
        Door.Box(0, .018f, -.035f, .78f, .025f, .17f, Hardware);
        Door.Box(0, .453f, .010f, .657f, .844f, .055f, Leaf);

        // A recessed lower panel, narrow frosted window and metal lever suit the wallpapered rooms.
        Door.Box(0, .257f, -.020f, .535f, .315f, .012f, Metal);
        Door.Box(0, .257f, -.028f, .506f, .286f, .006f, Leaf);
        Door.Box(-.040f, .640f, -.024f, .286f, .244f, .018f, Metal);
        Door.Box(-.040f, .640f, -.035f, .258f, .216f, .008f, ColorLerp(CurrentPalette.Lamp, Leaf, .52f));
        Door.Box(.251f, .455f, -.033f, .030f, .152f, .012f, Hardware);
        Door.Box(.224f, .469f, -.051f, .080f, .012f, .024f, Hardware);
        Door.Box(0, .079f, -.025f, .57f, .055f, .008f, Metal);
        Door.Box(0, .921f, -.057f, .304f, .066f, .020f, Hardware);
        return;
    }

    for (float CurrentSide : {-1.0f, 1.0f})
        Door.Box(CurrentSide * .324f, .456f, -.054f, .006f, .802f, .006f, Lamp);
    Door.Box(0, .042f, -.044f, .648f, .004f, .007f, Hardware);
    Door.Box(0, .921f, -.069f, .282f, .049f, .004f, Lamp);

    // Small block lettering stays readable without another font texture, shader or runtime asset.
    constexpr const char* Sign[] = {"111 101 111 111", "100 101 010 010", "110 010 010 010",
                                    "100 101 010 010", "111 101 111 010"};
    for (int Row = 0; Row < 5; ++Row)
        for (int Col = 0; Col < 15; ++Col)
            if (Sign[Row][Col] == '1')
                Door.Box((Col - 7) * .010f, .921f + (2 - Row) * .007f, -.072f, .008f, .005f, .002f,
                         Metal);
}
