# Code organization and reproducible runs

`GameDatabase` returns each operation's result directly. `RecordResult` distinguishes `Updated`, `Unchanged`, `Invalid` and `StorageError`. `BestTimeResult` combines a storage status with an optional time, so a missing score is different from a failed query. Profile operations return a status and their committed profile list. Callers update their displayed state only after success.

Schema creation and text-file imports live in `DatabaseMigrations.cpp`. Reusable SQL queries live in `DatabaseQueries.cpp`, and the scoped SQLite connection, statement and transaction owners live in `Sqlite.h`. The runtime database API accepts an alternative path for isolated tests. The database filename, schema and legacy migration markers remain compatible with existing saves.

`Renderer` draws world geometry and lighting. `RendererExit.cpp` draws the exit door in the normal material and emissive lighting passes. `HudRenderer` draws the HUD and minimap after the 3D pass. Embedded GLSL lives in `MazeShaders.h`; no external shader files are required at runtime. `Textures` is a move-only GPU resource owner. Borrowed handles are exposed through accessors for drawing, while destruction and move assignment release owned resources automatically. Keep resource owners inside the graphics context's lifetime.

The atmosphere shader evaluates up to sixteen nearby wall and ceiling fixtures per fragment, using rectangular light sources and smooth distance falloff. Packed maze-wall masks block light between corridors and are refreshed when the maze size or seed changes. The light list follows the camera; material lighting and emissive fixtures remain separate passes. Ambient illumination stays low enough to show the fixtures' light pools while retaining readable unlit passages.

Maze generation uses an explicitly seeded `std::mt19937`, independent of theme selection and decorative texture generation. To reproduce a loaded run, supply its seed and selected theme along with the same level parameters:

```cpp
Game reproduced(size, duration, original.mode(), original.seed(), original.theme());
```

Without a supplied theme, the seed also determines the default palette. The application owns a `RunThemeSelector`, which wraps `MazeThemeSelector` for non-repeating random themes across new runs. Supplying the same selector seed reproduces its entire sequence. No process-wide `srand` setup is needed.

`RunThemeSelector` reserves the next theme for menu previews and consumes it when a run starts. Menu navigation preserves that choice; restarting can consume the next theme directly. The current run keeps its palette through generation, gameplay, pause and results. Returning to the menus previews the next run's theme.

`MenuTheme::colorsFor` derives all interface materials from a maze palette. `MenuTheme::apply` updates the live color references used by drawing helpers and their default arguments. Main-menu and background color aliases must remain references so existing screens update without reloading fonts or recreating their geometry. The HUD uses the same palette factory for its active run.

The root `.clang-format` defines four-space indentation, a 100-column limit and consistent braces and spacing. Format edited C++ files before reviewing their diff. Existing explanatory comments remain above the code they describe.
