#include "../include/ClassErrorHandler.h"

/**
* Show a dialog message natively provided by OS for reporting.
* @param dialogType - SDL flag that corresponds to the type of message to be reported to the user.
* @param title - Message box title.
* @param msg - Message for display in message box.
* @param windowRef - SDL_Window variable to reference.
**/
void ClassErrorHandler::ShowDialogMsgOS(int dialogType, const char* title, const char* msg, SDL_Window * windowRef)
{
    const SDL_MessageBoxFlags msgArray[] = { SDL_MESSAGEBOX_ERROR, SDL_MESSAGEBOX_WARNING, SDL_MESSAGEBOX_INFORMATION };

    SDL_MessageBoxFlags flags = msgArray[0];
    if (dialogType >= 0 && dialogType < (int)(sizeof(msgArray) / sizeof(msgArray[0]))) {
        flags = msgArray[dialogType];
    }

    SDL_ShowSimpleMessageBox(
        flags,
        title,
        msg,
        windowRef
    );
}