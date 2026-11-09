#include "Renderer.h"
#include <cmath>

namespace {
// Position a conventional doorway against the exit cell's back wall, facing its approach.
struct ExitSpace {
    int last;
    bool north;

    // Convert exit-local across, height, and depth coordinates into world space according to the
    // final passage's orientation. This lets one authored doorway fit either approach direction.
    Vector3 point(float across, float height, float depth) const {
        return {(last + (north ? -across : .34f + depth)) * MazeGeometry::CellSize,
                height * MazeGeometry::CeilingHeight,
                (last + (north ? .34f + depth : across)) * MazeGeometry::CellSize};
    }

    // Draw an exit detail box using its local frame, swapping width and depth for the alternate
    // wall orientation. Scale horizontal dimensions by CellSize and height by CeilingHeight.
    void box(float across, float height, float depth, float width, float tall, float thick,
             Color color) const {
        DrawCube(point(across, height, depth), (north ? width : thick) * MazeGeometry::CellSize,
                 tall * MazeGeometry::CeilingHeight,
                 (north ? thick : width) * MazeGeometry::CellSize, color);
    }
};
}

// Draw the final corridor's exit architecture and its lit details using the run's theme and
// geometry. The glow flag selects its emissive contribution while the logical exit remains a maze
// cell.
void Renderer::drawExit(const Game& game, bool glow) {
    const int last = game.maze().size() - 1;
    const float dx = last - game.x() / MazeGeometry::CellSize;
    const float dz = last - game.z() / MazeGeometry::CellSize;
    if (dx * dx + dz * dz > 15.0f * 15.0f)
        return;
    const bool northOpen = game.maze().cell(last - 1, last) != '#';
    const bool westOpen = game.maze().cell(last, last - 1) != '#';
    const ExitSpace door{last, northOpen && (!westOpen || std::fabs(dz) >= std::fabs(dx))};
    const Color metal = ColorLerp(palette.trim, palette.floor, .20f);
    const Color leaf = ColorLerp(palette.trim, palette.wall, .55f);
    const Color hardware = ColorLerp(palette.accent, palette.paper, .12f);
    const Color lamp = ColorLerp(palette.wall, palette.lamp, .70f);

    if (!glow) {
        // Keep the familiar rectangular silhouette and trim proportions used elsewhere in the hall.
        for (float side : {-1.0f, 1.0f}) {
            door.box(side * .362f, .455f, 0, .065f, .90f, .10f, metal);
            door.box(side * .391f, .455f, -.052f, .008f, .90f, .010f, hardware);
        }
        door.box(0, .918f, 0, .79f, .078f, .10f, metal);
        door.box(0, .018f, -.035f, .78f, .025f, .17f, hardware);
        door.box(0, .453f, .010f, .657f, .844f, .055f, leaf);

        // A recessed lower panel, narrow frosted window and metal lever suit the wallpapered rooms.
        door.box(0, .257f, -.020f, .535f, .315f, .012f, metal);
        door.box(0, .257f, -.028f, .506f, .286f, .006f, leaf);
        door.box(-.040f, .640f, -.024f, .286f, .244f, .018f, metal);
        door.box(-.040f, .640f, -.035f, .258f, .216f, .008f, ColorLerp(palette.lamp, leaf, .52f));
        door.box(.251f, .455f, -.033f, .030f, .152f, .012f, hardware);
        door.box(.224f, .469f, -.051f, .080f, .012f, .024f, hardware);
        door.box(0, .079f, -.025f, .57f, .055f, .008f, metal);
        door.box(0, .921f, -.057f, .304f, .066f, .020f, hardware);
        return;
    }

    for (float side : {-1.0f, 1.0f})
        door.box(side * .324f, .456f, -.054f, .006f, .802f, .006f, lamp);
    door.box(0, .042f, -.044f, .648f, .004f, .007f, hardware);
    door.box(0, .921f, -.069f, .282f, .049f, .004f, lamp);

    // Small block lettering stays readable without another font texture, shader or runtime asset.
    constexpr const char* sign[] = {"111 101 111 111", "100 101 010 010", "110 010 010 010",
                                    "100 101 010 010", "111 101 111 010"};
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 15; ++col)
            if (sign[row][col] == '1')
                door.box((col - 7) * .010f, .921f + (2 - row) * .007f, -.072f, .008f, .005f, .002f,
                         metal);
}
