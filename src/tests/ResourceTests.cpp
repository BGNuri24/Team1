#include "../presentation/Textures.h"
#include <stdexcept>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<Textures> && !std::is_copy_assignable_v<Textures>);
static_assert(std::is_nothrow_move_constructible_v<Textures> &&
              std::is_nothrow_move_assignable_v<Textures>);

// Exercise exclusive texture ownership, move construction, move assignment, and replacement of an
// already loaded set inside a live graphics context. Verify moved-from objects become empty and
// cleanup does not double-unload handles.
void RunResourceTests() {
    Textures Original = CreateTextures(GetMazePalette(MazeTheme::Teal));
    if (!Original.Valid())
        throw std::runtime_error("resource fixture must create every material");
    const unsigned int CurrentWall = Original.GetWall().id;
    const unsigned int Marble = Original.Marble().id;
    Textures Moved(std::move(Original));
    if (Original.Valid() || Original.GetWall().id || Original.Marble().id || !Moved.Valid() ||
        Moved.GetWall().id != CurrentWall || Moved.Marble().id != Marble)
        throw std::runtime_error(
            "moving textures must transfer their handles and empty the source");
    Textures Replacement = CreateTextures(GetMazePalette(MazeTheme::Sage));
    Replacement = std::move(Moved);
    if (Moved.GetWall().id || Moved.Marble().id || !Replacement.Valid() ||
        Replacement.GetWall().id != CurrentWall || Replacement.Marble().id != Marble)
        throw std::runtime_error(
            "move assignment must replace owned resources without duplicating them");
    Replacement = std::move(Replacement);
    if (!Replacement.Valid())
        throw std::runtime_error("self-move must preserve the owned texture set");
}
