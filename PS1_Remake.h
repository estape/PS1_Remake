#pragma once

#include <SDL3/SDL.h>
#include "source/PS1_Remake/Engine/include/Core.h"
#include "source/PS1_Remake/Engine/include/MMC_Handle.h"
#include "source/PS1_Remake/Engine/include/SetAttr.h"
#include "source/PS1_Remake/Misc/include/ClassErrorHandler.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <string>

class PS1_Remake
{
public:
    PS1_Remake();
    ~PS1_Remake();

    void Run();

private:
    std::string StartEmulator(const std::string gamePath, int resWidth, int resHeight, bool fullScreen);

    // Função que vai rodar na Thread paralela
    void ListenToUE5();

    bool isGameRunning;

    // Variáveis de controle da Thread
    std::thread inputThread;
    std::atomic<bool> isListening;

    std::string TargetGamePath = "";
    int TargetResWidth = 0;
    int TargetResHeight = 0;
    bool TargetFullscreen = false;
};