#include "../include/CoreWrapper.h"

// --- DEFINIÇÕES IMPORTANTES ---
#define RETRO_DEVICE_TYPE_SHIFT 24
#define RETRO_DEVICE_NONE       0

// O que o Core PEDE (Tipos de Dados de Input)
#define RETRO_DEVICE_JOYPAD     1  // Botões Digitais
#define RETRO_DEVICE_ANALOG     5  // Eixos Analógicos (Sticks)

// O que nós CONECTAMOS (IDs de Hardware do PS1)
#define RETRO_DEVICE_PS_DIGITAL   1
#define RETRO_DEVICE_PS_DUALSHOCK 517 

// IDs dos Botões
#define RETRO_DEVICE_ID_JOYPAD_B        0 
#define RETRO_DEVICE_ID_JOYPAD_Y        1 
#define RETRO_DEVICE_ID_JOYPAD_SELECT   2
#define RETRO_DEVICE_ID_JOYPAD_START    3
#define RETRO_DEVICE_ID_JOYPAD_UP       4
#define RETRO_DEVICE_ID_JOYPAD_DOWN     5
#define RETRO_DEVICE_ID_JOYPAD_LEFT     6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT    7
#define RETRO_DEVICE_ID_JOYPAD_A        8 
#define RETRO_DEVICE_ID_JOYPAD_X        9 
#define RETRO_DEVICE_ID_JOYPAD_L        10
#define RETRO_DEVICE_ID_JOYPAD_R        11
#define RETRO_DEVICE_ID_JOYPAD_L2       12
#define RETRO_DEVICE_ID_JOYPAD_R2       13
#define RETRO_DEVICE_ID_JOYPAD_L3       14
#define RETRO_DEVICE_ID_JOYPAD_R3       15

// Comandos de Ambiente
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 10
#define RETRO_ENVIRONMENT_SET_HW_RENDER 14
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 31

// --- GLOBAIS ESTÁTICAS (Visíveis apenas neste arquivo) ---
static SDL_Texture* g_gameTexture = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_AudioStream* g_audioStream = nullptr;
static struct retro_hw_render_callback g_hw_render = {};
static SDL_Gamepad* s_activeGamepad = nullptr;

// Estado do Controle (Começa Digital para segurança)
static unsigned s_currentDeviceId = RETRO_DEVICE_PS_DIGITAL;
static bool s_btnTouchpadLastState = false;

// Ponteiro GLOBAL para a função da DLL (Para o InputPoll usar)
// Redefinimos o tipo aqui para uso local estático
typedef void (*global_set_port_t)(unsigned, unsigned);
static global_set_port_t g_set_controller_func = nullptr;

// --- Construtor ---
CoreWrapper::CoreWrapper() :
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
}

CoreWrapper::~CoreWrapper() { Unload(); }

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

// --- Callbacks ---
bool CoreWrapper::EnvironmentCallback(unsigned cmd, void* data) {
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
        struct retro_hw_render_callback* rec = (struct retro_hw_render_callback*)data;
        rec->context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
        rec->version_major = 3;
        rec->version_minor = 3;
        rec->context_reset = CoreWrapper::OnContextReset;
        rec->context_destroy = CoreWrapper::OnContextDestroy;
        g_hw_render = *rec;
        return false;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        struct retro_log_callback* cb = (struct retro_log_callback*)data;
        cb->log = CoreLog;
        return true;
    }
    default: return false;
    }
}

void CoreWrapper::VideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch) {
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

size_t RETRO_CALLCONV CoreWrapper::AudioSampleBatch(const int16_t* data, size_t frames) {
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

void CoreWrapper::InputPoll() {
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

    // --- LÓGICA DO TOUCHPAD (Troca de Modo) ---
    bool isPressed = SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_TOUCHPAD);

    // Toggle apenas quando aperta (Borda de subida)
    if (isPressed && !s_btnTouchpadLastState) {

        // Inverte: 1 -> 5 ou 5 -> 1
        s_currentDeviceId = (s_currentDeviceId == RETRO_DEVICE_PS_DIGITAL)
            ? RETRO_DEVICE_PS_DUALSHOCK
            : RETRO_DEVICE_PS_DIGITAL;

        // Se a função da DLL foi carregada com sucesso, usamos ela!
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

int16_t CoreWrapper::InputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || !s_activeGamepad) return 0;

    if (device == RETRO_DEVICE_ANALOG || device == RETRO_DEVICE_PS_DUALSHOCK) {
        if (s_currentDeviceId != RETRO_DEVICE_PS_DUALSHOCK) return 0;

        SDL_GamepadAxis targetAxis;

        // 1. SELECIONA O EIXO E A CONFIGURAÇÃO
        float deadzone, saturation;

        if (index == 0) {
            // Stick ESQUERDO (Movimento)
            targetAxis = (id == 0) ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_LEFTY;
            deadzone = 4000.0f;
            saturation = 24000.0f; // Passou daqui, é MÁXIMO!
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

        // 2. LÓGICA DE DEADZONE SIMPLES (Corta o centro)
        if (absVal < deadzone) return 0;

        // 3. LÓGICA DE SATURAÇÃO (Agressiva)
        // Se passar da saturação, força o valor máximo permitido pelo PS1 (32700)
        // Mantendo o sinal original (positivo ou negativo)
        if (absVal >= saturation) {
            return (val > 0) ? 32700 : -32700;
        }

        // 4. INTERPOLAÇÃO LINEAR (Para o meio do caminho)
        // Se está entre a deadzone e a saturação, escala suavemente
        // Ex: (Valor - Dead) / (Sat - Dead) * Max
        float normalized = (absVal - deadzone) / (saturation - deadzone);
        float finalVal = normalized * 32700.0f;

        // Devolve o sinal
        if (val < 0) finalVal = -finalVal;

        return (int16_t)finalVal;
    }

    // --- BOTÕES (Mantenha igual) ---
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

void CoreWrapper::OnContextReset() { SDL_Log("Contexto OpenGL Resetado!"); }
void CoreWrapper::OnContextDestroy() { SDL_Log("Contexto OpenGL Destruido!"); }

// --- Carregamento ---
bool CoreWrapper::LoadCore(const std::string& dllPath) {
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

    // --- CARREGAMENTO CRÍTICO ---
    // 1. Carrega o ponteiro para a Classe
    m_retro_set_controller_port_device = (retro_set_controller_port_device_t)load_sym("retro_set_controller_port_device");

    // 2. Copia o ponteiro para a Variável Global (para o InputPoll usar)
    g_set_controller_func = (global_set_port_t)m_retro_set_controller_port_device;

    auto set_video = (retro_set_video_refresh_t)load_sym("retro_set_video_refresh");
    auto set_audio = (retro_set_audio_sample_batch_t)load_sym("retro_set_audio_sample_batch");
    auto set_input = (retro_set_input_poll_t)load_sym("retro_set_input_poll");
    auto set_input_state = (retro_set_input_state_t)load_sym("retro_set_input_state");

    if (!m_retro_init || !m_retro_set_environment || !set_video || !set_audio) {
        SDL_Log("ERRO: Funcoes vitais ou Setters nao encontrados na DLL.");
        return false;
    }

    m_retro_set_environment(CoreWrapper::EnvironmentCallback);
    set_video(CoreWrapper::VideoRefresh);
    set_audio(CoreWrapper::AudioSampleBatch);
    set_input(CoreWrapper::InputPoll);
    set_input_state(CoreWrapper::InputState);

    SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };
    g_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (g_audioStream) SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(g_audioStream));

    m_retro_init();
    return true;
}

void CoreWrapper::EnableHardwareRenderer() { m_hw_render_enabled = true; }

bool CoreWrapper::LoadGame(const std::string& gamePath) {
    if (!m_retro_load_game) return false;
    struct retro_game_info info = { 0 };
    info.path = gamePath.c_str();

    if (m_retro_load_game(&info)) {
        // Começa garantindo que está no modo DIGITAL
        if (m_retro_set_controller_port_device) {
            m_retro_set_controller_port_device(0, RETRO_DEVICE_PS_DIGITAL);
        }
        return true;
    }
    return false;
}

void CoreWrapper::RunFrame() { if (m_retro_run) m_retro_run(); }

void CoreWrapper::Unload() {
    if (g_audioStream) {
        SDL_DestroyAudioStream(g_audioStream);
        g_audioStream = nullptr;
    }
    if (m_retro_deinit) m_retro_deinit();
    if (m_coreHandle) SDL_UnloadObject(static_cast<SDL_SharedObject*>(m_coreHandle));
    m_coreHandle = nullptr;
}

void CoreWrapper::InitVideo(SDL_Renderer* renderer) {
    g_renderer = renderer;
    g_gameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 1024, 512);
}

bool CoreWrapper::SaveState(const std::string& filepath) {
    // 1. O PRINT VEM PRIMEIRO (Antes de qualquer verificação)
    std::cout << "[DEBUG] >>> TENTANDO SALVAR O ESTADO <<<" << std::endl;

    // 2. Agora verificamos se as funções existem
    if (m_retro_serialize_size == nullptr) {
        std::cout << "[ERRO] Ponteiro 'serialize_size' esta NULO!" << std::endl;
        return false;
    }
    if (m_retro_serialize == nullptr) {
        std::cout << "[ERRO] Ponteiro 'serialize' esta NULO!" << std::endl;
        return false;
    }

    // 3. O resto do código continua...
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

bool CoreWrapper::LoadState(const std::string& filepath) {
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

    // 3. Verifica se bate com o que o Core espera (Segurança básica)
    size_t expectedSize = m_retro_serialize_size();
    if (fileSize != expectedSize) {
        SDL_Log("AVISO: Tamanho do Save (%zu) diferente do esperado pelo Core (%zu). Tentando mesmo assim...", fileSize, expectedSize);
    }

    // 4. Lê o arquivo para a memória
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