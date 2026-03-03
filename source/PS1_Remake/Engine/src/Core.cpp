#include "../include/Core.h"

// --- STATICS GLOBAL ---
static SDL_Texture* g_gameTexture = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_AudioStream* g_audioStream = nullptr;
static unsigned int g_fbo = 0; // Framebuffer Object (Invisible screen that where PS1 draw frames)
static unsigned int g_fbo_texture = 0;
static unsigned int g_last_width = 1024; // Default weight for FBO
static unsigned int g_last_height = 1024; // Default height for FBO
static struct retro_hw_render_callback g_hw_render = {};
static SDL_Gamepad* s_activeGamepad = nullptr;

/**
* Post-processing filters, options 0 to 5
* @param 0 "nearest" - No filter (Default)
* @param 1 "bilinear" - Basic smoothing, makes the image smoother, but may blur slightly.
* @param 2 "3-point" - An intermediate filter that attempts to balance sharpness and smoothing (Recommended for those who want a "cleaner" look without losing detail).
* @param 3 "xBR" - An advanced filter that smooths the image without blurring, ideal for games with many large pixels (May cause artifacts in games with many small details).
* @param 4 "SABR" - A high-quality filter that preserves detail and smooths the image, great for games with more complex graphics.
* @param 5 "JINC2" - The most advanced filter, offering the best possible image quality, but may be more demanding on hardware (Recommended for more powerful PCs).
* @return The string representing the chosen post-processing filter, based on the command value. "beetle_psx_hw_filter" or "beetle_psx_filter"
**/
static const char* posProcessFilters[] = { "nearest", "bilinear", "3-point", "xBR", "SABR", "JINC2" };

// Controller rumbble cache
static uint16_t s_rumble_strong = 0;
static uint16_t s_rumble_weak = 0;
static bool s_rumble_enabled = true;

// Set controller status (Digital is default)
static unsigned s_currentDeviceId = RETRO_DEVICE_PS_DIGITAL;
static bool s_btnTouchpadLastState = false;

// Ponteiro GLOBAL para a função da DLL (Para o InputPoll usar)
// Redefinimos o tipo aqui para uso local estático
typedef void (*global_set_port_t)(unsigned, unsigned);
static global_set_port_t g_set_controller_func = nullptr;

Core* Core::s_instance = nullptr;

// --- Construtor ---
Core::Core() :
    m_coreHandle(nullptr),
    m_hw_render_enabled(false),
    m_retro_init(nullptr),
    m_retro_deinit(nullptr),
    m_retro_load_game(nullptr),
    m_retro_run(nullptr),
    m_retro_set_environment(nullptr),
    m_retro_set_controller_port_device(nullptr),
    m_retro_serialize_size(nullptr),
    m_retro_serialize(nullptr),
    m_retro_unserialize(nullptr)
{
    s_instance = this;
}

Core::~Core() { Unload(); }

// --- Log ---
void RETRO_CALLCONV CoreLog(enum retro_log_level level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (level == RETRO_LOG_ERROR) fprintf(stderr, "[Libretro ERRO] ");
    else if (level == RETRO_LOG_WARN) fprintf(stdout, "[Libretro AVISO] ");
    else fprintf(stdout, "[Libretro INFO] ");
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

uintptr_t Core::GetCurrentFramebuffer() {
    return g_fbo; // Manda a GPU desenhar na nossa tela invis�vel!
}

retro_proc_address_t Core::GetProcAddress(const char* sym) {
    // Retornamos direto, sem o SDL_Log, para n�o poluir o console com os falsos positivos do OES.
    return (retro_proc_address_t)SDL_GL_GetProcAddress(sym);
}

// --- Callbacks ---
bool Core::EnvironmentCallback(unsigned cmd, void* data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
        static std::string systemPath;
        if (systemPath.empty()) {
            auto path = std::filesystem::current_path() / "system";
            systemPath = path.string();
            if (systemPath.back() != '\\' && systemPath.back() != '/') systemPath += "\\";
        }
        *(const char**)data = systemPath.c_str();
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
        static std::string savePath;
        if (savePath.empty()) {
            auto path = std::filesystem::current_path() / "saves";
            if (!std::filesystem::exists(path)) std::filesystem::create_directory(path);
            savePath = path.string();
            if (savePath.back() != '\\' && savePath.back() != '/') savePath += "\\";
        }
        *(const char**)data = savePath.c_str();
        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        if (!s_instance || !s_instance->m_hw_render_enabled) return false;

        struct retro_hw_render_callback* cb = (struct retro_hw_render_callback*)data;

        if (cb->context_type != RETRO_HW_CONTEXT_OPENGL_CORE &&
            cb->context_type != RETRO_HW_CONTEXT_OPENGL) {
            return false;
        }

        // 1. Copia as flags vitais do Core (Depth e Stencil)
        s_instance->m_hw_render_callback = *cb;

        // 2. Registra nossas fun��es
        s_instance->m_hw_render_callback.get_current_framebuffer = Core::GetCurrentFramebuffer;
        s_instance->m_hw_render_callback.get_proc_address = Core::GetProcAddress;

        cb->get_current_framebuffer = Core::GetCurrentFramebuffer;
        cb->get_proc_address = Core::GetProcAddress;

        SDL_Log("HW Render Negociado! Endereco do context_reset: %p", (void*)cb->context_reset);
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable* var = (struct retro_variable*)data;
        if (!var || !var->key) return false;
        std::string key = var->key;

        // Pula a BIOS da Sony (Fats Boot)
        if (key == "beetle_psx_hw_skip_bios" || key == "beetle_psx_skip_bios") {
            var->value = "disable";
            return true;
        }
		// Aumenta a resolu��o interna para 4x
        if (key == "beetle_psx_hw_internal_resolution" || key == "beetle_psx_internal_resolution") {
            var->value = "4x";
            return true;
        }
		// Ativa o renderizador hardware
        if (key == "beetle_psx_hw_renderer" || key == "beetle_psx_renderer") {
            var->value = "hardware";
            return true;
        }
        // Ativa filtros de p�s-processamento
        if (key == "beetle_psx_hw_filter" || key == "beetle_psx_filter") {
            var->value = posProcessFilters[2];
            return true;
        }

        // Desliga a tesoura de overscan
        if (key == "beetle_psx_hw_crop_overscan" || key == "beetle_psx_crop_overscan") {
            var->value = "enable";
            return true;
        }
		// Desliga o corte de bordas (que pode causar os famosos "pol�gonos tremendo" e "texturas derretendo" em alguns jogos)
        if (key == "beetle_psx_hw_image_crop" || key == "beetle_psx_image_crop") {
            var->value = "disabled";
            return true;
        }
        // Fim dos pol�gonos tremendo e texturas derretendo!
        if (key == "beetle_psx_hw_pgxp_mode" || key == "beetle_psx_pgxp_mode")
        {
            // "memory" � o modo mais seguro e est�vel. 
            // (Existe o "memory + CPU", mas pode causar crashes em alguns jogos).
            var->value = "memory";
            return true;
        }
        if (key == "beetle_psx_hw_pgxp_texture" || key == "beetle_psx_pgxp_texture")
        {
            // Ativa o "Perspective Correct Texturing" (Texturas cravadas na parede)
            var->value = "enabled";
            return true;
        }
        if (key == "beetle_psx_hw_pgxp_vertex" || key == "beetle_psx_pgxp_vertex")
        {
            // Alinha os v�rtices dos modelos 3D
            var->value = "enabled";
            return true;
        }

        return false;
    }
    case RETRO_ENVIRONMENT_SET_RUMBLE_INTERFACE: {
        struct retro_rumble_interface* ri = (struct retro_rumble_interface*)data;
        ri->set_rumble_state = Core::SetRumbleState;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        // --- AQUI ESTAVA O SEU CRASH! Se esta parte sumir, a DLL explode! ---
        struct retro_log_callback* cb = (struct retro_log_callback*)data;
        cb->log = CoreLog;
        return true;
    }
    default: return false;
    }
}

void Core::PresentFBO(SDL_Window* window) {
    if (!g_fbo) return;

    typedef void (*glBindFramebuffer_t)(unsigned int, unsigned int);
    typedef void (*glBlitFramebuffer_t)(int, int, int, int, int, int, int, int, unsigned int, unsigned int);
    typedef void (*glClearColor_t)(float, float, float, float);
    typedef void (*glClear_t)(unsigned int);
    typedef void (*glViewport_t)(int, int, int, int);

    auto my_glBindFramebuffer = (glBindFramebuffer_t)SDL_GL_GetProcAddress("glBindFramebuffer");
    auto my_glBlitFramebuffer = (glBlitFramebuffer_t)SDL_GL_GetProcAddress("glBlitFramebuffer");
    auto my_glClearColor = (glClearColor_t)SDL_GL_GetProcAddress("glClearColor");
    auto my_glClear = (glClear_t)SDL_GL_GetProcAddress("glClear");
    auto my_glViewport = (glViewport_t)SDL_GL_GetProcAddress("glViewport");

    if (!my_glBlitFramebuffer) return;

    int winW, winH;
    SDL_GetWindowSize(window, &winW, &winH);

    // Trava a tela em 4:3, simulando uma TV de tubo e lidando com resolu��es dinamicas do PS1
    float targetAspect = 4.0f / 3.0f;
    float windowAspect = (float)winW / (float)winH;

    int viewW = winW, viewH = winH;
    int viewX = 0, viewY = 0;

    if (windowAspect > targetAspect) {
        viewW = (int)(winH * targetAspect);
        viewX = (winW - viewW) / 2;
    }
    else {
        viewH = (int)(winW / targetAspect);
        viewY = (winH - viewH) / 2;
    }

    // Define o foco novamente a janela do usuario (ID 0)
    my_glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Pinta o fundo de preto (cria as barras laterais limpas)
    if (my_glClearColor) {
        my_glViewport(0, 0, winW, winH);
        my_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        my_glClear(GL_COLOR_BUFFER_BIT);
    }

    // O Copia-e-Cola M�gico do OpenGL!
    // Pega a imagem da Tela Invis�vel e "estica" perfeitamente no centro do monitor
    my_glBindFramebuffer(0x8CA8, g_fbo); // GL_READ_FRAMEBUFFER 
    my_glBindFramebuffer(0x8CA9, 0);     // GL_DRAW_FRAMEBUFFER

    my_glBlitFramebuffer(0, 0, g_last_width, g_last_height,
        viewX, viewY, viewX + viewW, viewY + viewH,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);

    // Muda o foco novamente para o FBO para renderizar o pr�ximo frame
    my_glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
}

void Core::VideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch) {
    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        g_last_width = width; // Largura exato do frame desenhado pelo PS1
        g_last_height = height; // Altura exato do frame desenhado pelo PS1

        if (s_instance) s_instance->m_frame_drawn = true;
        return;
    }

    if (!data || !g_gameTexture) return;

    SDL_Rect updateRect = { 0, 0, (int)width, (int)height };
    SDL_UpdateTexture(g_gameTexture, &updateRect, data, (int)pitch);

    int winW, winH;
    SDL_GetWindowSize(SDL_GetRenderWindow(g_renderer), &winW, &winH);

    float targetAspect = 4.0f / 3.0f;
    float windowAspect = (float)winW / (float)winH;
    SDL_FRect destRect;

    if (windowAspect > targetAspect) {
        destRect.h = (float)winH;
        destRect.w = destRect.h * targetAspect;
    }
    else {
        destRect.w = (float)winW;
        destRect.h = destRect.w / targetAspect;
    }
    destRect.x = (winW - destRect.w) / 2.0f;
    destRect.y = (winH - destRect.h) / 2.0f;

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_FRect srcRect = { 0, 0, (float)width, (float)height };
    SDL_RenderTexture(g_renderer, g_gameTexture, &srcRect, &destRect);
    SDL_RenderPresent(g_renderer);
}

size_t RETRO_CALLCONV Core::AudioSampleBatch(const int16_t* data, size_t frames) {
    if (!data || frames == 0) return 0;
    if (g_audioStream) {
        int queuedBytes = SDL_GetAudioStreamQueued(g_audioStream);
        const int MAX_LATENCY_BYTES = (int)(176400 * 0.06);
        if (queuedBytes < MAX_LATENCY_BYTES) {
            int bytes = static_cast<int>(frames * 4);
            SDL_PutAudioStreamData(g_audioStream, data, bytes);
        }
    }
    return frames;
}

void Core::InputPoll() {
    SDL_PumpEvents();

    if (!s_activeGamepad) {
        int count = 0;
        SDL_JoystickID* joys = SDL_GetGamepads(&count);
        if (count > 0) {
            s_activeGamepad = SDL_OpenGamepad(joys[0]);
            SDL_Log("InputPoll: Controle conectado: %s", SDL_GetGamepadName(s_activeGamepad));
        }
        else {
            return;
        }
    }

    // --- L�GICA DO TOUCHPAD (Troca Modo Digital/Modo anal�gico) ---
    bool isPressed = SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_TOUCHPAD);

    // Alterna entre os modos digital e anal�gico apenas quando aperta (Borda de subida)
    if (isPressed && !s_btnTouchpadLastState) {

        // Inverte: 1 -> 5 ou 5 -> 1
        s_currentDeviceId = (s_currentDeviceId == RETRO_DEVICE_PS_DIGITAL)
            ? RETRO_DEVICE_PS_DUALSHOCK
            : RETRO_DEVICE_PS_DIGITAL;

        // Se a fun��o da DLL foi carregada com sucesso, usamos ela!
        if (g_set_controller_func) {
            g_set_controller_func(0, s_currentDeviceId);

            if (s_currentDeviceId == RETRO_DEVICE_PS_DUALSHOCK)
                SDL_Log(">>> MODO DUALSHOCK (Analogico) ATIVADO <<<");
            else
                SDL_Log(">>> MODO DIGITAL (Padrao) ATIVADO <<<");
        }
    }
    s_btnTouchpadLastState = isPressed;
}

void Core::ToggleRumble() {
    s_rumble_enabled = !s_rumble_enabled; // Inverte o estado (Se era true, vira false e vice-versa)

    // Mostra no console para voc� saber se ligou ou desligou
    std::cout << "[CONTROLE] Vibracao: " << (s_rumble_enabled ? "LIGADA" : "DESLIGADA") << std::endl;
}

bool Core::SetRumbleState(unsigned port, enum retro_rumble_effect effect, uint16_t strength) {
    if (port != 0 || !s_activeGamepad) return false; // S� vibra o Player 1

    if (!s_rumble_enabled)
    {
		SDL_RumbleGamepad(s_activeGamepad, 0, 0, 0); // Desliga a vibra��o imediatamente
        return true;
    }
    // 1. Atualiza o cache do motor correspondente
    if (effect == RETRO_RUMBLE_STRONG)
    {
        s_rumble_strong = strength;
    }
    else if (effect == RETRO_RUMBLE_WEAK)
    {
        s_rumble_weak = strength;
    }

    /*
    * 2. Dispara a vibra��o na SDL3!
    * Enviamos a for�a dos dois motores. A dura��o � "infinita" (0xFFFF) 
    * porque o pr�prio emulador vai mandar strength = 0 quando for a hora de parar.
    */ 
    SDL_RumbleGamepad(s_activeGamepad, s_rumble_strong, s_rumble_weak, 0xFFFF);

    return true;
}

int16_t Core::InputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || !s_activeGamepad) return 0;

    if (device == RETRO_DEVICE_ANALOG || device == RETRO_DEVICE_PS_DUALSHOCK) {
        if (s_currentDeviceId != RETRO_DEVICE_PS_DUALSHOCK) return 0;

        SDL_GamepadAxis targetAxis;

        // 1. SELECIONA O EIXO E A CONFIGURA��O
        float deadzone, saturation;

        if (index == 0) {
            // Stick ESQUERDO (Movimento)
            targetAxis = (id == 0) ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_LEFTY;
            deadzone = 4000.0f;
            saturation = 24000.0f;
        }
        else {
            // Stick DIREITO (Mira)
            targetAxis = (id == 0) ? SDL_GAMEPAD_AXIS_RIGHTX : SDL_GAMEPAD_AXIS_RIGHTY;
            deadzone = 7000.0f;   // Deadzone maior pra evitar drift
            saturation = 26000.0f;
        }

        // Leitura bruta (-32768 a 32767)
        int rawVal = SDL_GetGamepadAxis(s_activeGamepad, targetAxis);

        // Trabalhamos com float para contas, mas mantemos o sinal
        float val = (float)rawVal;
        float absVal = std::abs(val);

        // 2. L�GICA DE DEADZONE SIMPLES (Corta o centro)
        if (absVal < deadzone) return 0;

        /* 3. L�GICA DE SATURA��O (Agressiva)
        Se passar da satura��o, for�a o valor m�ximo permitido pelo PS1 (32700)
        Mantendo o sinal original (positivo ou negativo) */
        if (absVal >= saturation) {
            return (val > 0) ? 32700 : -32700;
        }

         /* 4. INTERPOLA��O LINEAR(Para o meio do caminho)
         Se est� entre a deadzone e a satura��o, escala suavemente
         Ex: (Valor - Dead) / (Sat - Dead) * Max */
        float normalized = (absVal - deadzone) / (saturation - deadzone);
        float finalVal = normalized * 32700.0f;

        // Devolve o sinal
        if (val < 0) finalVal = -finalVal;

        return (int16_t)finalVal;
    }

    // --- BOT�ES (Mantenha igual) ---
    if (device == RETRO_DEVICE_JOYPAD) {
        switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_B:      return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_SOUTH);
        case RETRO_DEVICE_ID_JOYPAD_A:      return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_EAST);
        case RETRO_DEVICE_ID_JOYPAD_Y:      return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_WEST);
        case RETRO_DEVICE_ID_JOYPAD_X:      return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_NORTH);
        case RETRO_DEVICE_ID_JOYPAD_UP:     return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
        case RETRO_DEVICE_ID_JOYPAD_DOWN:   return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        case RETRO_DEVICE_ID_JOYPAD_LEFT:   return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        case RETRO_DEVICE_ID_JOYPAD_L:      return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        case RETRO_DEVICE_ID_JOYPAD_R:      return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        case RETRO_DEVICE_ID_JOYPAD_L2:     return SDL_GetGamepadAxis(s_activeGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 10000;
        case RETRO_DEVICE_ID_JOYPAD_R2:     return SDL_GetGamepadAxis(s_activeGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 10000;
        case RETRO_DEVICE_ID_JOYPAD_START:  return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_START);
        case RETRO_DEVICE_ID_JOYPAD_SELECT: return SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_BACK);
        case RETRO_DEVICE_ID_JOYPAD_L3:     return (s_currentDeviceId == RETRO_DEVICE_PS_DUALSHOCK) ? SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK) : 0;
        case RETRO_DEVICE_ID_JOYPAD_R3:     return (s_currentDeviceId == RETRO_DEVICE_PS_DUALSHOCK) ? SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK) : 0;
        default: return 0;
        }
    }
    return 0;
}

void Core::OnContextReset() { SDL_Log("Contexto OpenGL Resetado!"); }
void Core::OnContextDestroy() { SDL_Log("Contexto OpenGL Destruido!"); }

// --- CARREGAMENTO DO CORE ---
bool Core::LoadCore(const std::string& dllPath) {
    m_coreHandle = (void*)SDL_LoadObject(dllPath.c_str());
    if (!m_coreHandle) {
        SDL_Log("ERRO DLL: %s", SDL_GetError());
        return false;
    }

    auto load_sym = [&](const char* name) {
        return SDL_LoadFunction((SDL_SharedObject*)m_coreHandle, name);
        };

    m_retro_init = (retro_init_t)load_sym("retro_init");
    m_retro_deinit = (retro_deinit_t)load_sym("retro_deinit");
    m_retro_load_game = (retro_load_game_t)load_sym("retro_load_game");
    m_retro_run = (retro_run_t)load_sym("retro_run");
    m_retro_set_environment = (retro_set_environment_t)load_sym("retro_set_environment");

    m_retro_serialize_size = (retro_serialize_size_t)load_sym("retro_serialize_size");
    m_retro_serialize = (retro_serialize_t)load_sym("retro_serialize");
    m_retro_unserialize = (retro_unserialize_t)load_sym("retro_unserialize");

    m_retro_get_memory_data = (retro_get_memory_data_t)load_sym("retro_get_memory_data");
    m_retro_get_memory_size = (retro_get_memory_size_t)load_sym("retro_get_memory_size");

    if (!m_retro_get_memory_data || !m_retro_get_memory_size) {
        SDL_Log("AVISO: Funcoes de Memory Card (SRAM) nao encontradas na DLL.");
    }

    // Log para confirmar
    if (m_retro_serialize_size) {
        SDL_Log("Funcoes de Save State carregadas com sucesso!");
    }
    else {
        SDL_Log("ERRO: Funcoes de Save State NAO encontradas na DLL.");
    }

    if (!m_retro_serialize_size || !m_retro_serialize || !m_retro_unserialize) {
        SDL_Log("AVISO: Funcoes de Save State nao encontradas na DLL. F5/F9 nao funcionarao.");
    }

    // --- CARREGAMENTO CR�TICO ---
    // 1. Carrega o ponteiro para a Classe
    m_retro_set_controller_port_device = (retro_set_controller_port_device_t)load_sym("retro_set_controller_port_device");

    // 2. Copia o ponteiro para a Vari�vel Global (para o InputPoll usar)
    g_set_controller_func = (global_set_port_t)m_retro_set_controller_port_device;

    auto set_video = (retro_set_video_refresh_t)load_sym("retro_set_video_refresh");
    auto set_audio = (retro_set_audio_sample_batch_t)load_sym("retro_set_audio_sample_batch");
    auto set_input = (retro_set_input_poll_t)load_sym("retro_set_input_poll");
    auto set_input_state = (retro_set_input_state_t)load_sym("retro_set_input_state");

    if (!m_retro_init || !m_retro_set_environment || !set_video || !set_audio) {
        SDL_Log("ERRO: Funcoes vitais ou Setters nao encontrados na DLL.");
        return false;
    }

    m_retro_set_environment(Core::EnvironmentCallback);
    set_video(Core::VideoRefresh);
    set_audio(Core::AudioSampleBatch);
    set_input(Core::InputPoll);
    set_input_state(Core::InputState);

    SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };
    g_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (g_audioStream) SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(g_audioStream));

    m_retro_init();
    return true;
}

void Core::EnableHardwareRenderer() { m_hw_render_enabled = true; }

bool Core::LoadGame(const std::string& gamePath) {
    if (!m_retro_load_game) return false;
    struct retro_game_info info = { 0 };
    info.path = gamePath.c_str();

    // 1. CARREGA A ISO PRIMEIRO!
    // Durante o carregamento, a DLL vai preencher nossos ponteiros de GPU em background.
    if (m_retro_load_game(&info)) {

        if (m_retro_set_controller_port_device) {
            m_retro_set_controller_port_device(0, RETRO_DEVICE_PS_DIGITAL);
        }

        // 2. AGORA SIM! Damos a partida na GPU.
        if (m_hw_render_enabled) {

            // --- [NOVO] CRIA��O DA TELA INVIS�VEL (FBO) ---
            typedef void (*glGenFramebuffers_t)(int, unsigned int*);
            typedef void (*glBindFramebuffer_t)(unsigned int, unsigned int);
            typedef void (*glGenTextures_t)(int, unsigned int*);
            typedef void (*glBindTexture_t)(unsigned int, unsigned int);
            typedef void (*glTexImage2D_t)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
            typedef void (*glTexParameteri_t)(unsigned int, unsigned int, int);
            typedef void (*glFramebufferTexture2D_t)(unsigned int, unsigned int, unsigned int, unsigned int, int);

            auto my_glGenFramebuffers = (glGenFramebuffers_t)SDL_GL_GetProcAddress("glGenFramebuffers");
            auto my_glBindFramebuffer = (glBindFramebuffer_t)SDL_GL_GetProcAddress("glBindFramebuffer");
            auto my_glGenTextures = (glGenTextures_t)SDL_GL_GetProcAddress("glGenTextures");
            auto my_glBindTexture = (glBindTexture_t)SDL_GL_GetProcAddress("glBindTexture");
            auto my_glTexImage2D = (glTexImage2D_t)SDL_GL_GetProcAddress("glTexImage2D");
            auto my_glTexParameteri = (glTexParameteri_t)SDL_GL_GetProcAddress("glTexParameteri");
            auto my_glFramebufferTexture2D = (glFramebufferTexture2D_t)SDL_GL_GetProcAddress("glFramebufferTexture2D");

            if (my_glGenFramebuffers && !g_fbo) {
                typedef void (*glGenRenderbuffers_t)(int, unsigned int*);
                typedef void (*glBindRenderbuffer_t)(unsigned int, unsigned int);
                typedef void (*glRenderbufferStorage_t)(unsigned int, unsigned int, int, int);
                typedef void (*glFramebufferRenderbuffer_t)(unsigned int, unsigned int, unsigned int, unsigned int);

                auto my_glGenRenderbuffers = (glGenRenderbuffers_t)SDL_GL_GetProcAddress("glGenRenderbuffers");
                auto my_glBindRenderbuffer = (glBindRenderbuffer_t)SDL_GL_GetProcAddress("glBindRenderbuffer");
                auto my_glRenderbufferStorage = (glRenderbufferStorage_t)SDL_GL_GetProcAddress("glRenderbufferStorage");
                auto my_glFramebufferRenderbuffer = (glFramebufferRenderbuffer_t)SDL_GL_GetProcAddress("glFramebufferRenderbuffer");

                // Cria a Tela Invis�vel base
                my_glGenFramebuffers(1, &g_fbo);
                my_glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);

                // 1. A TINTA (Color Texture)
                my_glGenTextures(1, &g_fbo_texture);
                my_glBindTexture(GL_TEXTURE_2D, g_fbo_texture);
                my_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4096, 4096, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                my_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                my_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                my_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_fbo_texture, 0);

                // --- [A CURA] 2. O ESCUDO DE PROFUNDIDADE (Depth/Stencil Renderbuffer) ---
                if (my_glGenRenderbuffers) {
                    unsigned int rbo;
                    my_glGenRenderbuffers(1, &rbo);
                    my_glBindRenderbuffer(0x8D41, rbo); // 0x8D41 = GL_RENDERBUFFER
                    my_glRenderbufferStorage(0x8D41, 0x88F0, 4096, 4096); // 0x88F0 = GL_DEPTH24_STENCIL8
                    my_glFramebufferRenderbuffer(GL_FRAMEBUFFER, 0x821A, 0x8D41, rbo); // 0x821A = GL_DEPTH_STENCIL_ATTACHMENT
                }

                my_glBindFramebuffer(GL_FRAMEBUFFER, 0); // Desconecta para seguran�a
                SDL_Log("FBO Completo (Cor + Profundidade) criado com sucesso!");
            }

            auto safe_context_reset = m_hw_render_callback.context_reset;

            if (safe_context_reset) {
                safe_context_reset();
                SDL_Log("GPU Assumiu o controle! OpenGL Resetado com sucesso.");
            }
            else {
                SDL_Log("ERRO CRITICO: context_reset NULO apos o LoadGame! A GPU falhou.");
            }
        }
        return true;
    }
    return false;
}

void Core::RunFrame() { if (m_retro_run) m_retro_run(); }

void Core::Unload() {
    // 1� LUGAR: Desliga a Placa de V�deo (Enquanto a DLL ainda existe!)
    if (m_hw_render_enabled) {
        auto safe_context_destroy = m_hw_render_callback.context_destroy;
        if (safe_context_destroy) {
            safe_context_destroy();
            SDL_Log("Contexto OpenGL destruido com seguranca.");
        }
    }

    if (g_audioStream) {
        SDL_DestroyAudioStream(g_audioStream);
        g_audioStream = nullptr;
    }

    // 2� LUGAR: Desliga o sistema do emulador
    if (m_retro_deinit) {
        m_retro_deinit();
    }

    // 3� LUGAR: O Windows limpa a DLL ao fechar. Deixamos NULL por seguran�a.
    m_coreHandle = nullptr;
}

void Core::InitVideo(SDL_Renderer* renderer) {
    g_renderer = renderer;
    g_gameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 1024, 512);
}

bool Core::SaveState(const std::string& filepath) {
    // 1. O PRINT VEM PRIMEIRO (Antes de qualquer verifica��o)
    std::cout << "[DEBUG] >>> TENTANDO SALVAR O ESTADO <<<" << std::endl;

    // 2. Agora verificamos se as fun��es existem
    if (m_retro_serialize_size == nullptr) {
        std::cout << "[ERRO] Ponteiro 'serialize_size' esta NULO!" << std::endl;
        return false;
    }
    if (m_retro_serialize == nullptr) {
        std::cout << "[ERRO] Ponteiro 'serialize' esta NULO!" << std::endl;
        return false;
    }

    // 3. O resto do c�digo continua...
    size_t stateSize = m_retro_serialize_size();
    std::cout << "[DEBUG] Tamanho necessario: " << stateSize << " bytes." << std::endl;

    if (stateSize == 0) return false;

    std::vector<uint8_t> stateBuffer(stateSize);

    if (!m_retro_serialize(stateBuffer.data(), stateSize)) {
        std::cout << "[ERRO] Core falhou ao serializar." << std::endl;
        return false;
    }

    std::ofstream outFile(filepath, std::ios::binary);
    if (!outFile.is_open()) {
        std::cout << "[ERRO] Nao criou o arquivo: " << filepath << std::endl;
        return false;
    }

    outFile.write((const char*)stateBuffer.data(), stateSize);
    outFile.close();

    std::cout << "[SUCESSO] SALVO COM SUCESSO EM: " << filepath << std::endl;
    return true;
}

bool Core::LoadState(const std::string& filepath) {
    if (!m_retro_serialize_size || !m_retro_unserialize) return false;

    // 1. Abre o arquivo
    std::ifstream inFile(filepath, std::ios::binary);
    if (!inFile.is_open()) {
        SDL_Log("ERRO: Arquivo de save nao encontrado: %s", filepath.c_str());
        return false;
    }

    // 2. Descobre o tamanho do arquivo
    inFile.seekg(0, std::ios::end);
    size_t fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    // 3. Verifica se bate com o que o Core espera (Seguran�a b�sica)
    size_t expectedSize = m_retro_serialize_size();
    if (fileSize != expectedSize) {
        SDL_Log("AVISO: Tamanho do Save (%zu) diferente do esperado pelo Core (%zu). Tentando mesmo assim...", fileSize, expectedSize);
    }

    // 4. L� o arquivo para a mem�ria
    std::vector<uint8_t> stateBuffer(fileSize);
    inFile.read((char*)stateBuffer.data(), fileSize);
    inFile.close();

    // 5. Manda o Core engolir os dados
    if (!m_retro_unserialize(stateBuffer.data(), fileSize)) {
        SDL_Log("ERRO: Core rejeitou o Save State (Dados corrompidos ou versao diferente).");
        return false;
    }

    SDL_Log("SUCESSO: Estado carregado de %s", filepath.c_str());
    return true;
}

bool Core::LoadMemoryCard(const std::string& filepath) {
    if (!m_retro_get_memory_data || !m_retro_get_memory_size) return false;

    // Pede ao Core o tamanho do Memory Card e o ponteiro para a mem�ria
    size_t mcSize = m_retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void* mcData = m_retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);

    // O Beetle PSX normalmente aloca 128KB (131072 bytes) aqui
    if (mcSize == 0 || mcData == nullptr) {
        std::cout << "[AVISO] O Core nao ativou a interface de Memory Card." << std::endl;
        return false;
    }

    std::ifstream inFile(filepath, std::ios::binary);
    if (!inFile.is_open()) {
        std::cout << "[MEMORY CARD] Arquivo nao encontrado: " << filepath << " (O Core criara um novo vazio)." << std::endl;
        return false;
    }

    // Despeja os 128KB do disco direto na RAM do emulador
    inFile.read((char*)mcData, mcSize);
    inFile.close();

    std::cout << "[MEMORY CARD] Carregado com sucesso: " << filepath << " (" << mcSize << " bytes)." << std::endl;
    return true;
}

bool Core::SaveMemoryCard(const std::string& filepath) {
    if (!m_retro_get_memory_data || !m_retro_get_memory_size) return false;

    size_t mcSize = m_retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void* mcData = m_retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);

    if (mcSize == 0 || mcData == nullptr) return false;

    std::ofstream outFile(filepath, std::ios::binary);
    if (!outFile.is_open()) {
        std::cout << "[ERRO] Nao foi possivel gravar o Memory Card em: " << filepath << std::endl;
        return false;
    }

    // Pega os 128KB da RAM do emulador e salva no disco
    outFile.write((const char*)mcData, mcSize);
    outFile.close();

    std::cout << "[MEMORY CARD] Salvo com sucesso em: " << filepath << std::endl;
    return true;
}