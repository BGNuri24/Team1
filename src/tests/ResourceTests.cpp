#include "../presentation/Textures.h"
#include <stdexcept>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<Textures> && !std::is_copy_assignable_v<Textures>);
static_assert(std::is_nothrow_move_constructible_v<Textures> &&
              std::is_nothrow_move_assignable_v<Textures>);

// Exercise ownership transfer in the UI suite's live graphics context, including replacement of a
// loaded set.
void runResourceTests() {
    Textures original = createTextures(mazePalette(MazeTheme::Teal));
    if (!original.valid())
        throw std::runtime_error("resource fixture must create every material");
    const unsigned int wall = original.wall().id;
    Textures moved(std::move(original));
    if (original.valid() || original.wall().id || !moved.valid() || moved.wall().id != wall)
        throw std::runtime_error(
            "moving textures must transfer their handles and empty the source");
    Textures replacement = createTextures(mazePalette(MazeTheme::Sage));
    replacement = std::move(moved);
    if (moved.wall().id || !replacement.valid() || replacement.wall().id != wall)
        throw std::runtime_error(
            "move assignment must replace owned resources without duplicating them");
    replacement = std::move(replacement);
    if (!replacement.valid())
        throw std::runtime_error("self-move must preserve the owned texture set");
}
