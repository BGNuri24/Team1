#pragma once

// Identify a run's visual theme independently of rendering resources, maze geometry and score keys.
enum class MazeTheme { Slate, Teal, Crimson, Plum, Sage, GrandHotel };
inline constexpr int MazeThemeCount = 6;

// One palette for menus, new runs and retries until another collection is ready.
inline constexpr MazeTheme ActiveMazeTheme = MazeTheme::GrandHotel;
