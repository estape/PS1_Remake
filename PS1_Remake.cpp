#include "PS1_Remake.h"

// --- In future it will be filled from front-end ---
const std::string CORE_PATH = "mednafen_psx_hw_libretro.dll";
const std::string GAME_PATH = "D:/Users/Rodrigo/Emuladores/DuckStation/isos/Grand Theft Auto (USA).cue";

Core ps1Core; // Core functions from Engine
SetAttr PS1_Config; // PS1R configuration (SDL, OpenGL and Gamepad)
ErrorHandle errHandle; // Class with functions to handle errors.

// --- GLOBAL VARIABLES ---
SDL_Gamepad* g_gamepad = nullptr; // Vaviavel gamepad
int num_joysticks = 0; // Número de joysticks conectados

// --- FIX - FORCE WINDOWS TO USE THE DISCRETE GPU ---
#ifdef _WIN32
#include <windows.h>
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// --- EMULATOR START HERE ---

PS1_Remake::PS1_Remake()
{
    isGameRunning = false;
    std::cout << "[SYSTEM] PS1_Remake App Inicializado." << std::endl;
}

PS1_Remake::~PS1_Remake()
{
    std::cout << "[SYSTEM] PS1_Remake App Encerrado." << std::endl;
}

bool PS1_Remake::StartEmulator()
{
    const double TARGET_DT = 1000.0 / 60.0; // Set to 60 FPS (~16.66ms per frame)
    bool fastForward = false; // Fast-Forward mode (unlock 60 FPS Limit)
    bool running = true; // Main loop flag

    PS1_Config.InitSDL3(); // Initialize SDL with video and joystick support
    PS1_Config.SetOpenGL3_3(); // Set OpenGL core to 3.3 version (Required for Beetle HW)

    SDL_Window* window = SDL_CreateWindow("PS1 Remake",1280, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE); // Create window
    SDL_GLContext glContext = SDL_GL_CreateContext(window); // OpenGL Context (Turn on GPU) 
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&num_joysticks); // Listen Dualshock 4 controller
    SDL_Renderer* renderer = SDL_CreateRenderer(window, "opengl"); // Create OpenGL Renderer (connection between PS1 and GPU)
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
            window
        );

        return false;
    }

    g_gamepad = PS1_Config.GetGamepads(joysticks, num_joysticks); // Detect the first controller founded
    SDL_SetRenderVSync(renderer, 1); // Sync to screen in 60hz
    ps1Core.InitVideo(renderer); // Conecta o vídeo!

    if (!ps1Core.LoadCore(CORE_PATH)) // Carrega a DLL do emulador
    {
        SDL_Log("Erro fatal: Nao foi possivel carregar a DLL do emulador.");
        return false;
    }

    ps1Core.EnableHardwareRenderer(); // Start Hardware Mode
    SDL_GL_MakeCurrent(window, glContext); // Create context OpenGL before to load game
    if (!ps1Core.LoadGame(GAME_PATH)) // Load game, if GAME_PATH is not valid emulator will shutdown
    {
        SDL_Log("Erro: Nao foi possivel carregar o jogo (verifique o caminho ou a BIOS).");
        return false;
    }

    // --- MAIN LOOP ---
    ps1Core.LoadMemoryCard("D:/Users/Rodrigo/Documents/Dev/VisualStudio/Desktop/PS1_Remake/memcard1.mcr"); // Load a Memory Card file to PS1 Slot 1, will be filled by UI in future

    // ***Running the program***
    while (running) {
        uint64_t startParams = SDL_GetTicks(); // Marca o tempo de início do frame

        // --- POLL EVENT LOOP ---
        // *** Every input will be check it and registered in log ***
        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_EVENT_QUIT) // Check if window was closed
            {
                running = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN) // Check with key on keyboard was pressed
            {

                // Debug para confirmar que a tecla chegou limpa A debug to confirm that keyboard key was pressed successfully
                std::cout << "TECLA: " << SDL_GetKeyName(event.key.key) << std::endl;

                // F5: Save
                if (event.key.key == SDLK_F5) {
                    std::cout << "Salvando Estado..." << std::endl;
                    ps1Core.SaveState("quicksave.state");
                }

                // F9: Load
                if (event.key.key == SDLK_F9) {
                    std::cout << "Carregando Estado..." << std::endl;
                    ps1Core.LoadState("quicksave.state");
                }

                // ESC: Exit
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }

                // TAB: Fast-Forward (Hold it)
                if (event.key.key == SDLK_TAB) {
                    fastForward = true;
                }
            }

            // Verifica as teclas soltas Check with keyboard keys was released
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_TAB) {
                    fastForward = false;
                }
            }
        } // <--- END POLL EVENT --->

        // --- RUN FRAME ---
        ps1Core.m_frame_drawn = false;
        ps1Core.RunFrame(); // Core already calculate Viewport!

        // *** Set FPS count (Sleep) ***
        if (!fastForward) {
            uint64_t endParams = SDL_GetTicks();
            double elapsedMS = (double)(endParams - startParams);

            if (elapsedMS < TARGET_DT) {
                SDL_Delay((Uint32)(TARGET_DT - elapsedMS));
            }
        }

        // --- FIX - Stable image ---
        if (ps1Core.m_frame_drawn)
        {
            ps1Core.PresentFBO(window); // Set and show FBO Screen (Grab the invisible screen and project it perfectly centered)
            SDL_GL_SwapWindow(window); // Switch buffers (Show image to screen)
        }
    } // <--- End program running (Main loop) --->

    // Automatic save Memory Card file before closing emulator.
    std::cout << "Desligando o console... Salvando Memory Card..." << std::endl;

    ps1Core.Unload();
    if (g_gamepad) SDL_CloseGamepad(g_gamepad);
    SDL_Quit();

    return true;
}

void PS1_Remake::Run()
{
    // Temporarily we will start the emulator directly util UI is ready
    std::cout << "[SYSTEM] Iniciando fluxo principal..." << std::endl;
    StartEmulator();
}

bool PS1_Remake::StartUI()
{
    // UI will be start here
    return true;
}

int main(int argc, char* argv[]) {
    PS1_Remake app;
    app.Run();
    return 0;
}