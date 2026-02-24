#include <SDL3/SDL.h>
#include "source/PS1_Remake/Engine/include/Core.h"
#include "source/PS1_Remake/Engine/include/SetAttr.h"
#include <iostream>

// --- Futuramente a ser definido pelo front-end ---
const std::string CORE_PATH = "mednafen_psx_hw_libretro.dll";
const std::string GAME_PATH = "D:/Users/Rodrigo/Emuladores/DuckStation/isos/Grand Theft Auto (USA).cue";
// ---------------------------------------------

SetAttr PS1_Config; // Configurações do PS1 (SDL, OpenGL e Gamepad)

// --- VARIÁVEIS GLOBAIS ---
Core ps1Core; //Crie a instância
SDL_Gamepad* g_gamepad = nullptr; // Vaviavel gamepad
int num_joysticks = 0; // Número de joysticks conectados

#ifdef _WIN32
#include <windows.h>
// Força o driver da NVIDIA a usar a GPU Dedicada em vez da Integrada
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
// -----------------------

int main(int argc, char* argv[]) {
    
    const double TARGET_DT = 1000.0 / 60.0; // Controle de FPS (60 FPS = ~16.66ms por frame)
	bool fastForward = false; // Modo Fast-Forward
	bool running = true; // Flag para o loop principal

	PS1_Config.InitSDL3(); //Inicializa SDL com suporte a Vídeo e Joystick
	PS1_Config.SetOpenGL3_3(); // [IMPORTANTE] Configura o OpenGL 3.3 Core (Requisito do Beetle HW)

    SDL_Window* window = SDL_CreateWindow("PS1 Remake",1280, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE); // Cria a Janela
    SDL_GLContext glContext = SDL_GL_CreateContext(window); //Cria o Contexto OpenGL (Liga a GPU)
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&num_joysticks); //Detectar o controle DUALSHOCK 4
	SDL_Renderer* renderer = SDL_CreateRenderer(window, "opengl"); // Cria o Renderizador OpenGL (A ponte entre o PS1 e a GPU)
	SDL_Event event; // Variável para eventos (input)

	if (!renderer) // Verifica se o renderizador foi criado com sucesso
    {
        SDL_Log("Erro fatal: Nao foi possivel criar o renderizador OpenGL. Verifique se sua placa de video suporta OpenGL 3.3 ou superior.");
        std::string erroMsg = "Falha ao criar o Motor Gráfico (Renderer)!\nMotivo: ";
        erroMsg += SDL_GetError();
        std::cout << "[ERRO FATAL] " << erroMsg << std::endl;
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,       // Ícone de X vermelho do SO
            "Erro Fatal - PS1 Remake",  // Título da janelinha
            erroMsg.c_str(),            // O texto do erro convertido para C-string
            window                      // A janela principal do emulador (pode ser nullptr se a janela ainda não existir)
        );

        // 4. Aborta a missão
        return false;
    }

    g_gamepad = PS1_Config.GetGamepads(joysticks, num_joysticks); // Abre o primeiro controle que achar
    SDL_SetRenderVSync(renderer, 1); // Sincronizar com o monitor (60Hz)
    ps1Core.InitVideo(renderer); // Conecta o vídeo!

	if (!ps1Core.LoadCore(CORE_PATH)) // Carrega a DLL do emulador
    {
        SDL_Log("Erro fatal: Nao foi possivel carregar a DLL do emulador.");
        return -1;
    }

    ps1Core.EnableHardwareRenderer(); // Ativa o modo HW
    SDL_GL_MakeCurrent(window, glContext); // Criar o contexto OpenGL ANTES de carregar o jogo
    if (!ps1Core.LoadGame(GAME_PATH)) // Carrega o Jogo
    {
        SDL_Log("Erro: Nao foi possivel carregar o jogo (verifique o caminho ou a BIOS).");
        return -1;
    }

	// --- LOOP PRINCIPAL ---
    ps1Core.LoadMemoryCard("D:/Users/Rodrigo/Documents/Dev/VisualStudio/Desktop/PS1_Remake/memcard1.mcr"); // Carrega o Memory Card

    // ***Execução do programa***
    while (running) {
        uint64_t startParams = SDL_GetTicks(); // Marca o tempo de início do frame

        // --- LOOP DE EVENTOS (POLL EVENT) ---
        // ***Tudo que é input TEM que ser verificado aqui dentro!***
        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_EVENT_QUIT) // Verifica se fechou a janela
            {
                running = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN) // Verifica teclas pressionadas
            {

                // Debug para confirmar que a tecla chegou limpa
                std::cout << "TECLA: " << SDL_GetKeyName(event.key.key) << std::endl;

                // F5: SALVAR
                if (event.key.key == SDLK_F5) {
                    std::cout << "Salvando Estado..." << std::endl;
                    ps1Core.SaveState("quicksave.state");
                }

                // F9: CARREGAR
                if (event.key.key == SDLK_F9) {
                    std::cout << "Carregando Estado..." << std::endl;
                    ps1Core.LoadState("quicksave.state");
                }

                // ESC: SAIR
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }

				// TAB: Fast-Forward (Segure para acelerar)
                if (event.key.key == SDLK_TAB) {
                    fastForward = true;
				}
            }

            // Verifica as teclas soltas
			if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_TAB) {
                    fastForward = false;
                }
            }
        } // <--- FIM DO WHILE DO POLL EVENT (Muito importante fechar aqui)

        // --- PROCESSAMENTO DO FRAME ---
        ps1Core.m_frame_drawn = false; // Abaixa a bandeira
        ps1Core.RunFrame(); // O Core faz os próprios cálculos de Viewport lá dentro!

        // *** LIMITADOR DE VELOCIDADE (Sleep) ***
        if (!fastForward) {
            uint64_t endParams = SDL_GetTicks();
            double elapsedMS = (double)(endParams - startParams);

            if (elapsedMS < TARGET_DT) {
                SDL_Delay((Uint32)(TARGET_DT - elapsedMS));
            }
        }

        // --- Correção para imagem estável ---
        if (ps1Core.m_frame_drawn)
        {
            ps1Core.PresentFBO(window); // Pega a tela invisível e projeta perfeitamente centrada!
			SDL_GL_SwapWindow(window); // Troca os buffers (Mostra a imagem na tela)
        }
	} // <--- Fim do da execução do programa (Loop Principal)

    // ANTES DE FECHAR O EMULADOR (Save automático do Memory Card)
    std::cout << "Desligando o console... Salvando Memory Card..." << std::endl;

    ps1Core.Unload();
    if (g_gamepad) SDL_CloseGamepad(g_gamepad);
    SDL_Quit();
    return 0;
}