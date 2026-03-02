#include <SDL3/SDL.h>
#include <string>

/**
* Show a dialog message natively provided by OS for reporting.
* @param dialogType - SDL flag that corresponds to the type of message to be reported to the user.
* @param title - Message box title.
* @param msg - Message for display in message box.
* @param windowRef - SDL_Window variable to reference.
**/
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