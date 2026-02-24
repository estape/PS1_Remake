#include <SDL3/SDL.h>
#include <string>

static void ShowDialogMsgOS(int dialogType, const char* title, const char* msg, SDL_Window * windowRef)
{
    SDL_MessageBoxFlags* msgArray [] = {SDL_MESSAGEBOX_ERROR, SDL_MESSAGEBOX_WARNING, SDL_MESSAGEBOX_INFORMATION}
    SDL_ShowSimpleMessageBox(
        msgArray[dialogType],
        title,
        msg,
        windowRef
    );
}