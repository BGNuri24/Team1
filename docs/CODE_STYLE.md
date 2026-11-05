# Code organization and reproducible runs

`GameDatabase` returns each operation's result directly. `RecordResult` distinguishes `Updated`, `Unchanged`, `Invalid` and `StorageError`. `BestTimeResult` combines a storage status with an optional time, so a missing score is different from a failed query. Profile operations return a status and their committed profile list. Callers update their displayed state only after success.

Schema creation and text-file imports live in `DatabaseMigrations.cpp`. Reusable SQL queries live in `DatabaseQueries.cpp`, and the scoped SQLite connection, statement and transaction owners live in `Sqlite.h`. The runtime database API accepts an alternative path for isolated tests. The database filename, schema and legacy migration markers remain compatible with existing saves.

`Renderer` draws world geometry and lighting. `HudRenderer` draws the HUD and minimap after the 3D pass. Embedded GLSL lives in `MazeShaders.h`; no external shader files are required at runtime. `Textures` is a move-only GPU resource owner. Borrowed handles are exposed through accessors for drawing, while destruction and move assignment release owned resources automatically. Keep resource owners inside the graphics context's lifetime.

Maze generation uses an explicitly seeded `std::mt19937`, independent of theme selection and decorative texture generation. To reproduce a loaded run, supply its seed and selected theme along with the same level parameters:

```cpp
Game reproduced(size, duration, original.mode(), original.seed(), original.theme());
```

Without a supplied theme, the seed also determines the default palette. The application owns a `MazeThemeSelector` for non-repeating random themes across new runs. Supplying the same selector seed reproduces its entire sequence. No process-wide `srand` setup is needed.

The root `.clang-format` defines four-space indentation, a 100-column limit and consistent braces and spacing. Format edited C++ files before reviewing their diff. Existing explanatory comments remain above the code they describe.
