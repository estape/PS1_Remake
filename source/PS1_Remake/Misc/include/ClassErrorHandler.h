#pragma once

#include <SDL3/SDL.h>
#include <string>

class ClassErrorHandler
{
public:
    static void ShowDialogMsgOS(int dialogType, const char* title, const char* msg, SDL_Window * windowRef);
};