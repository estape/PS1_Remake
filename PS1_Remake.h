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

    // O maestro geral da aplicação
    void Run();

private:
    // Os dois grandes "corações" do projeto
    bool StartEmulator();
    bool StartUI();

    // Variável de controle de estado
    bool isGameRunning;
};
