#include <SDL3/SDL.h>

class SetAttr
{
    public:
        int InitSDL3();
        static void SetOpenGL3_3();
        SDL_Gamepad* GetGamepads(SDL_JoystickID* SDL_Joystick, int num);
};