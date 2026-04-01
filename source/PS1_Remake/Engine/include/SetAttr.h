#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>

extern std::unordered_map<std::string, std::string> g_gameDatabase;

class SetAttr
{
public:
    static void LoadGameDatabase(const std::string& csvFilePath);
    static std::string ExtractPS1GameID(const std::string& binPath);
    int InitSDL3();
    static void SetOpenGL3_3();
    SDL_Gamepad* GetGamepads(SDL_JoystickID* SDL_Joystick, int num);
};