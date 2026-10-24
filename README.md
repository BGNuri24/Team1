# Team1 — Maze Game

C++20 maze game using raylib 5.5.

All game and C++ test targets use C++20 in Debug and Release. A compiler with
C++20 support is required. The Visual Studio solution and VS Code build task
share the `/std:c++20` setting in `windows/Build.props`; VS Code IntelliSense is
also configured for C++20. The alternative CMake build requires C++20 and
disables compiler-specific C++ extensions. Raylib's own `.c` files compile as C.

## Build and run in Visual Studio (no CMake)

1. Install Visual Studio 2026 with **Desktop development with C++** and a Windows SDK.
2. Open **MazeGame.sln** and choose **Debug | x64** or **Release | x64**.
3. Right-click **MazeGame** and select **Set as Startup Project**.
4. Press **Ctrl+Shift+B** to build, then **F5** to debug or **Ctrl+F5** to run.

The solution builds the game, raylib, and all three test executables directly with
MSBuild. On the first build, `windows/Get-Raylib.ps1` downloads the pinned raylib
5.5 sources from its official GitHub repository. Internet access is needed only
for this restore. The library is kept in the ignored `third_party/raylib` folder,
with its license, and is compiled locally. CMake is not invoked.

Visual Studio runs the game from the project folder so `players.txt` and
`best_times.txt` stay there. Executables are written to
`build-vs/x64/Debug/` or `build-vs/x64/Release/`.

To build from Developer PowerShell for Visual Studio:

```powershell
msbuild .\MazeGame.sln /m /p:Configuration=Debug /p:Platform=x64
```

## Run from VS Code

Open the `Team1` folder in VS Code and install Microsoft's **C/C++** extension
(`ms-vscode.cpptools`) if it is not installed already. Visual Studio's C++ build
tools are still required.

Open **Run and Debug** with **Ctrl+Shift+D**, select **Run MazeGame**, and click
the green play button. **F5** starts debugging; **Ctrl+F5** runs without debugging.
The launch configuration builds the game with MSBuild before starting it and
uses the project folder for player records. It works regardless of the active
editor file. **Ctrl+Shift+B** builds without launching.

## Tests

After building the solution, run from the project folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\windows\RunTests.ps1
```

For Release, add `-Configuration Release`. Tests use separate directories under
`build-vs` and leave the game's player records untouched. When debugging a test
project in Visual Studio, its working directory is also isolated.

To prepare raylib offline from an existing 5.5 source checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\windows\Get-Raylib.ps1 -SourcePath C:\path\to\raylib
```

The existing `CMakeLists.txt` remains available as an alternative build setup.
When adding game source files, add them through Visual Studio's **Add > Existing
Item** or **Add > New Item** so they are included in `windows/MazeGame.vcxproj`.

## File layout

```text
Team1/
  src/                 Game code: data, logic, presentation, shared
  .vscode/             VS Code Run and Debug configuration
  tests/               Automated test source
  windows/             Visual Studio projects, settings, and helper scripts
  third_party/raylib/  Restored library source and license (ignored by Git)
  build-vs/            Generated Visual Studio output (ignored by Git)
  build-refactor/      Generated CMake output (ignored by Git)
  MazeGame.sln         Open this solution in Visual Studio
  CMakeLists.txt       Optional CMake build configuration
  players.txt          Saved player profiles
  best_times.txt       Saved player records
  memory.txt           Local project notes (ignored by Git)
  README.md            Setup and usage instructions
```

Only the game's maintained code belongs in `src`. Tests, dependencies, build
configuration, generated output, and saved data have their own locations.
The game's `.vcxproj.filters` file mirrors the `src` folders in Solution Explorer;
it lives beside the project in `windows`, rather than beside the source files.

raylib: https://github.com/raysan5/raylib/tree/5.5 (zlib license).
