#pragma once
#include "MazePalette.h"
#include "raylib.h"

// Own one material set exclusively and release its GPU handles before the graphics context is
// closed.
class Textures {
public:
    // Use the compiler-generated default constructor and the member initializers declared in this
    // type. No external resources are acquired here; later operations establish the object's
    // usable state.
    Textures() = default;
    // Adopt supplied material handles as exclusive ownership, or transfer them from another
    // Textures object when moving. The owner releases its handles automatically; copying is
    // disabled to prevent duplicate GPU deletion.
    Textures(Texture2D wall, Texture2D floor, Texture2D ceiling, Texture2D marble) noexcept;
    // Unload every owned material through reset when this resource set leaves scope. The owner
    // must be destroyed before CloseWindow tears down the graphics context.
    ~Textures();
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    Textures(const Textures&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    Textures& operator=(const Textures&) = delete;
    // Adopt supplied material handles as exclusive ownership, or transfer them from another
    // Textures object when moving. The owner releases its handles automatically; copying is
    // disabled to prevent duplicate GPU deletion.
    Textures(Textures&& other) noexcept;
    // Release the destination's current materials before transferring handles from the source,
    // leaving the moved-from object empty. Handle self-assignment without unloading resources
    // that are still owned by this object.
    Textures& operator=(Textures&& other) noexcept;
    // Borrow the wall texture handle for drawing without transferring ownership. The reference
    // remains usable only while this resource set retains that material.
    const Texture2D& wall() const { return wallTexture; }
    // Borrow the carpet atlas handle for drawing without transferring ownership. Its field and
    // border regions are interpreted by the renderer, and the Textures object remains responsible
    // for unloading it.
    const Texture2D& floor() const { return floorTexture; }
    // Borrow the ceiling texture handle for drawing without transferring ownership. The caller
    // must not unload it or retain it after this set is replaced or destroyed.
    const Texture2D& ceiling() const { return ceilingTexture; }
    // Borrow the stone material handle used by pilasters and related detail geometry. Ownership
    // remains with this Textures object, which releases it during reset or destruction.
    const Texture2D& marble() const { return marbleTexture; }
    // Require nonzero handles for every material before the renderer accepts a replacement set.
    // This prevents a partially created palette from displacing the currently usable resources.
    bool valid() const;

private:
    // Unload any nonzero material handles and clear the stored values afterward. Destruction and
    // move assignment share this cleanup path so released textures are not deleted twice.
    void reset() noexcept;
    Texture2D wallTexture{}, floorTexture{}, ceilingTexture{}, marbleTexture{};
};

// Create the wall, floor, ceiling, and marble GPU materials and return them in one exclusively
// owned Textures object. A valid graphics context is required before loading or generating these
// resources.
Textures createTextures(const MazePalette& palette = mazePalette(ActiveMazeTheme));
