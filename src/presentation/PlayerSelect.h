#pragma once
#include <string>
#include "Screen.h"
#include "../logic/Players.h"

class PlayerSelect {
public:
    explicit PlayerSelect(Players& players);
    // LEVEL_SELECT = player chosen, MAIN_MENU = back, PLAYER_SELECT = stay
    Screen update();      
    void draw() const;
private:
    int optionCount() const;

    Players& players;
    int selected;
    // true while typing a new name
    bool typing;          
    std::string text;
    // 0 none, 1 invalid name, 2 duplicate
    int error;            
};