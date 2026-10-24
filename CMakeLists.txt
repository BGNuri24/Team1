cmake_minimum_required(VERSION 3.14)
project(MazeGame CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5)
FetchContent_MakeAvailable(raylib)

add_executable(MazeGame
    src/main.cpp
    src/data/MazeData.cpp
    src/data/PlayerStore.cpp
    src/logic/Game.cpp
    src/logic/Players.cpp
    src/presentation/Input.cpp
    src/presentation/Application.cpp
    src/presentation/ApplicationScreens.cpp
    src/presentation/Renderer.cpp
    src/presentation/Textures.cpp
    src/presentation/Ui.cpp
    src/presentation/MainMenu.cpp
    src/presentation/ControlsScreen.cpp
    src/presentation/PlayerSelect.cpp
    src/presentation/LevelSelect.cpp
    src/presentation/PauseMenu.cpp
    src/presentation/EndScreen.cpp
    src/presentation/MazeGenerationScreen.cpp)
target_link_libraries(MazeGame raylib)

if(BUILD_TESTING)
    add_executable(MazeGameTests
        tests/CoreTests.cpp
        src/data/MazeData.cpp
        src/data/PlayerStore.cpp
        src/logic/Players.cpp)
    add_test(NAME MazeGameCoreTests COMMAND MazeGameTests)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-data")
    set_tests_properties(MazeGameCoreTests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-data")

    add_executable(MazeGameLogicTests tests/GameTests.cpp src/logic/Game.cpp src/data/MazeData.cpp)
    target_link_libraries(MazeGameLogicTests PRIVATE raylib)
    add_test(NAME MazeGameLogicTests COMMAND MazeGameLogicTests)

    add_executable(MazeInputTests tests/InputTests.cpp src/presentation/Input.cpp)
    target_include_directories(MazeInputTests PRIVATE $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>)
    add_test(NAME MazeInputTests COMMAND MazeInputTests)
endif()
