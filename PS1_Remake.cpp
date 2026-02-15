#include <SDL3/SDL.h>
#include "all/source/include/CoreWrapper.h" // Ajuste o caminho para o correto
#include "all/PS1_Remake/Model/include/SetAttr.h"
#include <iostream>

// Futuramente a ser definido pelo front-end
const std::string CORE_PATH = "mednafen_psx_hw_libretro.dll";
const std::string GAME_PATH = "D:/Users/Rodrigo/Emuladores/DuckStation/isos/Syphon Filter 2 (USA) (Disc 1).cue"; // Seu jogo aqui!

SetAttr PS1_Config;

//Crie a instância
CoreWrapper ps1Wrapper;

//Variável global do gamepad
SDL_Gamepad* g_gamepad = nullptr;

// Controle de FPS (60 FPS = ~16.66ms por frame)
const double TARGET_DT = 1000.0 / 60.0;

int num_joysticks = 0;

int main(int argc, char* argv[]) {
    //Inicializa SDL com suporte a Vídeo e Joystick
	PS1_Config.InitSDL3();
    
    //[IMPORTANTE] Configura o OpenGL 3.3 Core (Requisito do Beetle HW)
	PS1_Config.SetOpenGL3_3();

    // 3. Cria a Janela
    SDL_Window* window = SDL_CreateWindow("PS1 Remake",
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    //Cria o Contexto OpenGL (Liga a GPU)
    SDL_GLContext glContext = SDL_GL_CreateContext(window);

    //Detectar o controle DUALSHOCK 4
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&num_joysticks);

    // Abre o primeiro controle que achar
    g_gamepad = PS1_Config.GetGamepads(joysticks, num_joysticks);

    //Logo depois de criar o renderer...
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) { /*...*/ }

    //Sincronizar com o monitor (60Hz)
    SDL_SetRenderVSync(renderer, 1);

    ps1Wrapper.InitVideo(renderer); // <--- Conecta o vídeo!
    // ---------------------
    // 
    // --- AVISO SOBRE A BIOS ---
    // Adicionar no EnvironmentCallback o caso RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY
    // para retornar o caminho completo da sua pasta "system".

    //Tenta carregar o cartucho (DLL)
    if (!ps1Wrapper.LoadCore(CORE_PATH)) {
        SDL_Log("Erro fatal: Nao foi possivel carregar a DLL do emulador.");
        return -1;
    }

    //Ativa o modo HW
    ps1Wrapper.EnableHardwareRenderer();

    //Carrega o Jogo
    if (!ps1Wrapper.LoadGame(GAME_PATH)) {
        SDL_Log("Erro: Nao foi possivel carregar o jogo (verifique o caminho ou a BIOS).");
        return -1;
    }

    //O Loop Principal
    bool running = true;
    SDL_Event event;

    // ***Execução do programa***
    while (running) {
        // Marca o tempo de início do frame
        uint64_t startParams = SDL_GetTicks();

        // --- LOOP DE EVENTOS (A "Esteira") ---
        // Tudo que é input TEM que ser verificado aqui dentro!
        while (SDL_PollEvent(&event)) {

            // 1. Verifica se fechou a janela
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }

            // 2. Verifica teclas (AGORA NO LUGAR CERTO)
            if (event.type == SDL_EVENT_KEY_DOWN) {

                // Debug para confirmar que a tecla chegou limpa
                std::cout << "TECLA: " << SDL_GetKeyName(event.key.key) << std::endl;

                // F5: SALVAR
                if (event.key.key == SDLK_F5) {
                    std::cout << "Salvando Estado..." << std::endl;
                    ps1Wrapper.SaveState("quicksave.state");
                }

                // F9: CARREGAR
                if (event.key.key == SDLK_F9) {
                    std::cout << "Carregando Estado..." << std::endl;
                    ps1Wrapper.LoadState("quicksave.state");
                }

                // ESC: SAIR
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
        } // <--- FIM DO WHILE DO POLL EVENT (Muito importante fechar aqui)

        // --- PROCESSAMENTO DO FRAME ---

        // Roda 1 frame do PS1
        ps1Wrapper.RunFrame(); // (Ou core.RunFrame(), use o nome da sua variável)

        // *** LIMITADOR DE VELOCIDADE (Sleep) ***
        uint64_t endParams = SDL_GetTicks();
        double elapsedMS = (double)(endParams - startParams);

        if (elapsedMS < TARGET_DT) {
            SDL_Delay((Uint32)(TARGET_DT - elapsedMS));
        }
    }

    ps1Wrapper.Unload();
    if (g_gamepad) SDL_CloseGamepad(g_gamepad);
    SDL_Quit();
    return 0;
}