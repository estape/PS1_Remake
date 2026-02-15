#include "../include/SetAttr.h"
#include <utility> // para std::pair

SDL_Gamepad* _gamepad = nullptr;

int SetAttr::InitSDL3()
{
    //Inicializa SDL com suporte a Vídeo e Joystick
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        SDL_Log("Erro ao iniciar SDL: %s", SDL_GetError());
        return -1;
    }

	return 0;
}

void SetAttr::SetOpenGL3_3()
{
    //[IMPORTANTE] Configura o OpenGL 3.3 Core (Requisito do Beetle HW)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

SDL_Gamepad* SetAttr::GetGamepads(SDL_JoystickID* SDL_Joystick, int num)
{
    if (SDL_Joystick && num > 0) {
        // Abre o primeiro controle que achar
        _gamepad = SDL_OpenGamepad(SDL_Joystick[0]);
        if (_gamepad) {
            SDL_Log("Controle conectado: %s", SDL_GetGamepadName(_gamepad));
            return _gamepad;
        }
        return _gamepad;
    }
    return _gamepad;
}