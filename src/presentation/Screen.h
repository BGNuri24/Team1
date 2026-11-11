#pragma once

// Identify the application states used to dispatch screen updates, drawing and navigation transitions.
enum class Screen { MainMenu, CONTROLS, PlayerSelect, LevelSelect, GENERATING, PLAYING, PAUSED, END, QUIT };