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
void runResourceTests() {
    Textures original = createTextures(mazePalette(MazeTheme::Teal));
    if (!original.valid())
        throw std::runtime_error("resource fixture must create every material");
    const unsigned int wall = original.wall().id;
    const unsigned int marble = original.marble().id;
    Textures moved(std::move(original));
    if (original.valid() || original.wall().id || original.marble().id || !moved.valid() ||
        moved.wall().id != wall || moved.marble().id != marble)
        throw std::runtime_error(
            "moving textures must transfer their handles and empty the source");
    Textures replacement = createTextures(mazePalette(MazeTheme::Sage));
    replacement = std::move(moved);
    if (moved.wall().id || moved.marble().id || !replacement.valid() ||
        replacement.wall().id != wall || replacement.marble().id != marble)
        throw std::runtime_error(
            "move assignment must replace owned resources without duplicating them");
    replacement = std::move(replacement);
    if (!replacement.valid())
        throw std::runtime_error("self-move must preserve the owned texture set");
}
