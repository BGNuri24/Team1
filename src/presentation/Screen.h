#pragma once

// Identify the application states used to dispatch screen updates, drawing and navigation transitions.
enum class Screen { MAIN_MENU, CONTROLS, PLAYER_SELECT, LEVEL_SELECT, GENERATING, PLAYING, PAUSED, END, QUIT };