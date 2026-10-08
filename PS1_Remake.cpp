#include "PS1_Remake.h"

Core ps1Core;                // Core functions from Engine
SetAttr PS1_Config;          // PS1R configuration (SDL, OpenGL and Gamepad)
MMC_Handle MemoryCardHandle; // Class to handle Memory Card operations (Format, Read, Write, etc)
ClassErrorHandler errHandle; // Class with functions to handle errors.

// --- GLOBAL VARIABLES ---
SDL_Gamepad *g_gamepad = nullptr; // Vaviavel gamepad
int num_joysticks = 0;            // Número de joysticks conectados
bool running = true;              // Main loop flag

enum EnumPS1ucmdParams
{
    NONE,
    QUIT,
    START,
    SAVESTATE,
    LOADSTATE,
};

// Internal state for PS1UCMD
EnumPS1ucmdParams states;

EnumPS1ucmdParams GetPS1ucmdParam(const std::string &cmdParam)
{
    if (cmdParam.compare("QUIT") == 0)
        return QUIT;
    else if (cmdParam.compare("START") == 0)
        return START;
    else if (cmdParam.compare("SAVESTATE") == 0)
        return SAVESTATE;
    else if (cmdParam.compare("LOADSTATE") == 0)
        return LOADSTATE;
    else
        throw std::invalid_argument("PS1LOG: Falied to set internal cmdParam");
}

// --- FIX - FORCE WINDOWS TO USE THE DISCRETE GPU ---
#ifdef _WIN32
#include <windows.h>
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// --- EMULATOR START HERE ---

PS1_Remake::PS1_Remake()
{
    isGameRunning = false;
    isListening = false;

    // Desliga o buffer do console. Garante que os couts cheguem na UE5 instantaneamente!
    setvbuf(stdout, NULL, _IONBF, 0);

    std::cout << "PS1LOG:PS1 Remake Inicializado." << std::endl;
}

PS1_Remake::~PS1_Remake()
{
    std::cout << "PS1LOG:PS1 Remake Encerrado." << std::endl;
}

void PS1_Remake::ListenToUE5()
{
    std::string rawCommand = "";
    std::string command = "";
    std::string cmdParam = "";
    size_t cmdParamLen = 0;

    // Fica rodando em background enquanto o emulador estiver ligado
    while (isListening)
    {
        // Pega o que vier do WriteToProcess() da Unreal
        if (std::getline(std::cin, rawCommand)) // <-- Arrumar, se a UE5 crashar vai consumir 100% da CPU.
        {
            if (rawCommand.starts_with("PS1UCMD:"))
            {
                rawCommand = rawCommand.substr(8);
                cmdParamLen = rawCommand.find("|");

                if (cmdParamLen != std::string::npos)
                {
                    command = rawCommand.substr(0, cmdParamLen);   // Get the part before "|".
                    cmdParam = rawCommand.substr(cmdParamLen + 1); // Get the part after "|".
                }

                else
                {
                    std::cout << "PS1LOG:PS1UCMD invalid!" << std::endl;
                    rawCommand = "";
                    command = "";
                    cmdParam = "";
                    cmdParamLen = 0;
                }

                if (!command.empty())
                {
                    switch (GetPS1ucmdParam(command))
                    {
                    case QUIT:
                        states = QUIT;
                        break;

                    case START:
                        if (!cmdParam.empty())
                        {
                            std::cout << "PS1LOG:%s" << cmdParam << std::endl;

                            /** * Format: GamePath;ResWidth;ResHeight;FullscreenFlag;posProcessFilters;internalResolutions
                             * e.g.: C:\Game.cue;1920;1080;1;0;0
                             **/
                            size_t pos1 = cmdParam.find(";");           // Separa GamePath de ResWidth
                            size_t pos2 = cmdParam.find(";", pos1 + 1); // Separa ResWidth de ResHeight
                            size_t pos3 = cmdParam.find(";", pos2 + 1); // Separa ResHeight de FullscreenFlag
                            size_t pos4 = cmdParam.find(";", pos3 + 1); // Separa FullscreenFlag de posProcessFilters
                            size_t pos5 = cmdParam.find(";", pos4 + 1); // Separa posProcessFilters de internalResolutions

                            if (pos1 != std::string::npos && pos2 != std::string::npos && pos3 != std::string::npos &&
                                pos4 != std::string::npos && pos5 != std::string::npos)
                            {
                                TargetGamePath = cmdParam.substr(0, pos1);
                                try
                                {
                                    TargetResWidth = std::stoi(cmdParam.substr(pos1 + 1, pos2 - pos1 - 1));
                                    TargetResHeight = std::stoi(cmdParam.substr(pos2 + 1, pos3 - pos2 - 1));
                                }
                                catch (const std::exception &e)
                                {
                                    std::cout << "PS1RES:StartEmulator|FALSE" << std::endl;
                                    std::cout << "PS1LOG:Invalid resolution values in CMD string: " << e.what() << std::endl;
                                    break;
                                }

                                std::string fsFlag = cmdParam.substr(pos3 + 1, pos4 - pos3 - 1);
                                if (fsFlag == "1" || fsFlag == "true")
                                {
                                    std::cout << "PS1LOG:Fullscreen mode enabled by command." << std::endl;
                                    TargetFullscreen = true;
                                }
                                else
                                {
                                    std::cout << "PS1LOG:Fullscreen mode disabled by command." << std::endl;
                                    TargetFullscreen = false;
                                }

                                std::string posProcessValue = cmdParam.substr(pos4 + 1, pos5 - pos4 - 1);
                                std::string internalResValue = cmdParam.substr(pos5 + 1);

                                if (!posProcessValue.empty() && !internalResValue.empty())
                                {
                                    int tempPosProcessValue = 0;
                                    int tempInternalResValue = 0;

                                    try
                                    {
                                        tempPosProcessValue = std::stoi(posProcessValue);
                                    }
                                    catch (const std::exception &)
                                    {
                                        std::cout << "PS1LOG:Pos Process invalid, it will keep to 0" << std::endl;
                                    }

                                    try
                                    {
                                        tempInternalResValue = std::stoi(internalResValue);
                                    }
                                    catch (const std::exception &)
                                    {
                                        std::cout << "PS1LOG:Internal Resolution invalid, it will keep to 0" << std::endl;
                                    }

                                    ps1Core.CallConfigVideoSet(tempPosProcessValue, tempInternalResValue);
                                }
                                else
                                {
                                    std::cout << "PS1LOG:Invalid post-process filter value in CMD string. Expected 0-5, got: " << posProcessValue << std::endl;
                                }

                                if (TargetGamePath.ends_with(".cue") || TargetGamePath.ends_with(".CUE"))
                                {
                                    states = START;
                                }
                                else
                                {
                                    std::cout << "PS1RES:StartEmulator|FALSE" << std::endl;
                                    std::cout << "PS1LOG:Game path must be .cue. Given path: " << TargetGamePath << std::endl;
                                }
                            }
                            else
                            {
                                std::cout << "PS1RES:StartEmulator|FALSE" << std::endl;
                                std::cout << "PS1LOG:Missing parameters in CMD string" << std::endl;
                            }
                        }
                        break;
                    }
                }
                else
                {
                    std::cout << "PS1LOG:PS1UCMD empty!" << std::endl;
                }
            }
            else
            {
                std::cout << "PS1LOG:" << cmdParam << std::endl;
            }
        }
        else
        {
            std::cout << "PS1LOG:Failed to read command from UE5. Stopping listening thread." << std::endl;
            isListening = false; // Stop listening thread if reading fails (e.g., UE5 closed)
        }
    }
}

std::string PS1_Remake::StartEmulator(const std::string gamePath, int resWidth, int resHeight, bool fullScreen)
{
    // Path to the emulator core DLL (Beetle PSX HW)
    const std::string CORE_PATH = "mednafen_psx_hw_libretro.dll";
    // Set to 60 FPS (~16.66ms per frame)
    const double TARGET_DT = 1000.0 / 60.0;
    // Fast-Forward mode (unlock 60 FPS Limit)
    bool fastForward = false;
    // Set fullscreen or windowed mode
    SDL_WindowFlags flagWindow;

    // Initialize SDL with video and joystick support
    PS1_Config.InitSDL3();
    // Set OpenGL core to 3.3 version (Required for Beetle HW)
    PS1_Config.SetOpenGL3_3();

    if (resWidth < 758 || resHeight < 573)
    {
        resWidth = 758;
        resHeight = 573;

        std::cout << "PS1RES:Resolution|W758;H573" << std::endl;
        std::cout << "PS1LOG:Resolution is less then width 758 or height 573, setting to 758x573: " << resWidth << "x" << resHeight << std::endl;
    }

    if (fullScreen)
    {
        flagWindow = SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    else
    {
        flagWindow = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    // Create emulator window
    SDL_Window *window = SDL_CreateWindow("PS1 Remake", resWidth, resHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    // OpenGL Context (Turn on GPU)
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    // Listen SDL3 compatible controllers
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&num_joysticks);
    // Create OpenGL Renderer (connection between PS1 and GPU)
    SDL_Renderer *renderer = SDL_CreateRenderer(window, "opengl");
    // SDL Event (For input and window events)
    SDL_Event event;

    if (!renderer) // Check if OpenGL renderer was created successfully
    {
        SDL_Log("Erro fatal: Nao foi possivel criar o renderizador OpenGL. Verifique se sua placa de video suporta OpenGL 3.3 ou superior.");
        std::string erroMsg = "Falha ao criar o Motor Gráfico (Renderer)!\nMotivo: ";
        erroMsg += SDL_GetError();
        std::cout << "[ERRO FATAL] " << erroMsg << std::endl;
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "Erro Fatal - PS1 Remake",
            erroMsg.c_str(),
            window);

        return "PS1RES:StartEmulator|FALSE\nPS1LOG:Falied to create OpenGL renderer";
    }

    g_gamepad = PS1_Config.GetGamepads(joysticks, num_joysticks); // Detect the first controller founded
    SDL_SetRenderVSync(renderer, 1);                              // Sync to screen in 60hz
    ps1Core.InitVideo(renderer);                                  // Start video system of the emulator core (Beetle PSX HW)

    if (!ps1Core.LoadCore(CORE_PATH)) // Load the emulator core DLL (Beetle PSX HW)
    {
        std::cout << "PS1LOG:Failed to load emulator core DLL!" << std::endl;

        return "\nPS1RES:StartEmulator|FALSE";
    }

    ps1Core.EnableHardwareRenderer();      // Start Hardware Mode
    SDL_GL_MakeCurrent(window, glContext); // Create context OpenGL before to load game
    if (!ps1Core.LoadGame(gamePath))
    {
        return "PS1RES:StartEmulator|FALSE\nPS1LOG:Can't read game path";
    } // Try to load the game, if fails show error message and close emulator safely

    // --- MAIN LOOP --- (Running the program)
    while (running)
    {
        uint64_t startParams = SDL_GetTicks(); // Get the start time of the frame (for FPS control)

        // --- POLL EVENT LOOP ---
        // *** Every input will be check it and registered in log ***
        while (SDL_PollEvent(&event))
        {

            if (event.type == SDL_EVENT_QUIT) // Check if window was closed
            {
                running = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN) // Check with key on keyboard was pressed
            {

                // A debug to confirm that keyboard key was pressed successfully
                std::cout << "PS1LOG:TECLA: " << SDL_GetKeyName(event.key.key) << std::endl;

                // F5: Save
                if (event.key.key == SDLK_F5 || states == SAVESTATE)
                {
                    std::cout << "Salvando Estado..." << std::endl;
                    ps1Core.SaveState("quicksave.state");
                }

                // F9: Load
                if (event.key.key == SDLK_F9 || states == LOADSTATE)
                {
                    std::cout << "Carregando Estado..." << std::endl;
                    ps1Core.LoadState("quicksave.state");
                }

                // ESC: Shutdown
                if (event.key.key == SDLK_ESCAPE || states == QUIT)
                {
                    std::cout << "PS1LOG:Shutdown" << std::endl;
                    running = false;
                }

                // 1: Restart
                if (event.key.key == SDLK_1 || states == QUIT)
                {
                    std::cout << "PS1ECMD|RESTART" << std::endl;
                    running = false;
                }

                // TAB: Fast-Forward (Hold it)
                if (event.key.key == SDLK_TAB)
                {
                    fastForward = true;
                }
            }

            // Verifica as teclas soltas Check with keyboard keys was released
            if (event.type == SDL_EVENT_KEY_UP)
            {
                if (event.key.key == SDLK_TAB)
                {
                    fastForward = false;
                }
            }
        } // <--- END POLL EVENT --->

        // --- RUN FRAME ---
        ps1Core.m_frame_drawn = false;
        ps1Core.RunFrame(); // Core already calculate Viewport!

        // *** Set FPS count (Sleep) ***
        if (!fastForward)
        {
            uint64_t endParams = SDL_GetTicks();
            double elapsedMS = (double)(endParams - startParams);

            if (elapsedMS < TARGET_DT)
            {
                SDL_Delay((Uint32)(TARGET_DT - elapsedMS));
            }
        }

        // --- FIX - Stable image ---
        if (ps1Core.m_frame_drawn)
        {
            ps1Core.PresentFBO(window); // Set and show FBO Screen (Grab the invisible screen and project it perfectly centered)
            SDL_GL_SwapWindow(window);  // Switch buffers (Show image to screen)
        }
    } // <--- End program running (Main loop) --->

    // Automatic save Memory Card file before closing emulator.
    std::cout << "PS1LOG:Turning off the ps1_console... Saving Memory Card" << std::endl;

    ps1Core.Unload();
    if (g_gamepad)
        SDL_CloseGamepad(g_gamepad);
    SDL_Quit();

    std::cout << "PS1LOG:StartEmulator was finished with success" << std::endl;
    return "PS1RES:StartEmulator|TRUE";
}

void PS1_Remake::Run()
{
    std::cout << "PS1LOG:[SYSTEM] Starting main flow..." << std::endl;

    isListening = true;
    inputThread = std::thread(&PS1_Remake::ListenToUE5, this);

    // LOOP PRINCIPAL DE ESPERA (Aguardando comando da UE5)
    while (isListening)
    {
        if (states == START)
        {
            std::string resultLog = StartEmulator(TargetGamePath, TargetResWidth, TargetResHeight, TargetFullscreen);
            std::cout << resultLog << std::endl;
            states = QUIT;
        }

        if (states == QUIT)
        {
            isListening = false;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (inputThread.joinable())
    {
        inputThread.detach();
    }
}

int main(int argc, char *argv[])
{
    PS1_Remake app;
    app.Run();
    return 0;
}