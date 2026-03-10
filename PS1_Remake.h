#pragma once

#include <SDL3/SDL.h>
#include "source/PS1_Remake/Engine/include/Core.h"
#include "source/PS1_Remake/Engine/include/SetAttr.h"
#include <iostream>

class PS1_Remake
{
public:
    PS1_Remake();
    ~PS1_Remake();

    void Run();

private:
    bool StartEmulator();
    bool StartUI();

    bool isGameRunning;
};
