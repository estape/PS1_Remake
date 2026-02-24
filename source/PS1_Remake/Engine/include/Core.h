#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <functional>
#include <iostream>
#include <vector>
#include <filesystem>
#include <cstdio>   
#include <cstdarg>  
#include <algorithm> 
#include <fstream>

// --- Definições da API Libretro ---
#define RETRO_API_VERSION 1

#ifndef RETRO_CALLCONV
#define RETRO_CALLCONV
#endif

// --- DEFINIÇÕES IMPORTANTES ---
#define RETRO_DEVICE_TYPE_SHIFT 24
#define RETRO_DEVICE_NONE       0

#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA8 0x8058
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_COLOR_BUFFER_BIT 0x00004000

// O que o Core PEDE (Tipos de Dados de Input)
#define RETRO_DEVICE_JOYPAD     1  // Botões Digitais
#define RETRO_DEVICE_ANALOG     5  // Eixos Analógicos (Sticks)

// O que nós CONECTAMOS (IDs de Hardware do PS1)
#define RETRO_DEVICE_PS_DIGITAL   1
#define RETRO_DEVICE_PS_DUALSHOCK 517 

// --- DEFINIÇÕES DE VIBRAÇÃO (RUMBLE) ---
#define RETRO_ENVIRONMENT_SET_RUMBLE_INTERFACE 23

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
#define RETRO_ENVIRONMENT_GET_VARIABLE 15
#define RETRO_HW_FRAME_BUFFER_VALID ((void *)-1)
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 31

// Memory Cards
#define RETRO_MEMORY_SAVE_RAM 0

// Níveis de Log
enum retro_log_level {
    RETRO_LOG_DEBUG = 0,
    RETRO_LOG_INFO,
    RETRO_LOG_WARN,
    RETRO_LOG_ERROR,
    RETRO_LOG_DUMMY = 2147483647
};

// Tipos de Contexto de Hardware
enum retro_hw_context_type {
    RETRO_HW_CONTEXT_NONE = 0,
    RETRO_HW_CONTEXT_OPENGL = 1,
    RETRO_HW_CONTEXT_OPENGLES2 = 2,
    RETRO_HW_CONTEXT_OPENGL_CORE = 3,
    RETRO_HW_CONTEXT_VULKAN = 6
};

// Efeitos de Rumble (Vibração)
enum retro_rumble_effect {
    RETRO_RUMBLE_STRONG = 0, // Motor pesado (Baixa frequência)
    RETRO_RUMBLE_WEAK = 1    // Motor leve (Alta frequência)
};

// --- INTERFACE DE LOG (Para o Core enviar mensagens de log) ---
struct retro_log_callback {
    void (*log)(enum retro_log_level level, const char* fmt, ...);
};

// --- INTERFACE DE RUMBLE (VIBRAÇÃO) ---
struct retro_rumble_interface {
    bool (*set_rumble_state)(unsigned port, enum retro_rumble_effect effect, uint16_t strength);
};

// --- [TYPEDEFS DA GPU OFICIAIS] ---
typedef void (*retro_proc_address_t)(void);
typedef void (*retro_hw_context_reset_t)(void);
typedef uintptr_t(*retro_hw_get_current_framebuffer_t)(void);
typedef retro_proc_address_t(*retro_hw_get_proc_address_t)(const char* sym);

struct retro_hw_render_callback {
    enum retro_hw_context_type context_type;
    retro_hw_context_reset_t context_reset;

    // Olha como o get_current e o get_proc_address subiram de posição!
    retro_hw_get_current_framebuffer_t get_current_framebuffer;
    retro_hw_get_proc_address_t get_proc_address;

    bool depth;
    bool stencil;
    bool bottom_left_origin;
    unsigned version_major;
    unsigned version_minor;
    bool cache_context;

    // Olha o context_destroy aqui no final, onde ele deveria estar!
    retro_hw_context_reset_t context_destroy;
    retro_hw_context_reset_t context_destroy_custom;
    retro_hw_context_reset_t context_reset_custom;
};

struct retro_variable {
    const char* key;
    const char* value;
};

struct retro_game_info {
    const char* path;
    const void* data;
    size_t size;
    const char* meta;
};

class Core {
public:
    bool m_frame_drawn = false;

    Core();
    ~Core();

    bool LoadCore(const std::string& dllPath);
    bool LoadGame(const std::string& gamePath);
    void RunFrame();
    void Unload();
    void EnableHardwareRenderer();
    void InitVideo(SDL_Renderer* renderer);

    // --- FBO NO MONITOR ---
    void PresentFBO(SDL_Window* window);

    static bool EnvironmentCallback(unsigned cmd, void* data);
    static void VideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch);
    static void InputPoll();
    static int16_t InputState(unsigned port, unsigned device, unsigned index, unsigned id);
    static bool SetRumbleState(unsigned port, enum retro_rumble_effect effect, uint16_t strength);
    static size_t AudioSampleBatch(const int16_t* data, size_t frames);
    static void OnContextReset();
    static void OnContextDestroy();

    // --- FUNÇÕES ESTÁTICAS PARA A GPU ---
    static Core* s_instance; // A ponte para acessar a classe!
    static uintptr_t GetCurrentFramebuffer();
    static retro_proc_address_t GetProcAddress(const char* sym);

    bool SaveState(const std::string& filepath);
    bool LoadState(const std::string& filepath);

    // --- MEMORY CARD ---
    bool LoadMemoryCard(const std::string& filepath);
    bool SaveMemoryCard(const std::string& filepath);

	// --- ATIVAÇÃO/DESATIVAÇÃO RUMBLE (Vibração) ---
    void ToggleRumble();

private:
    void* m_coreHandle;
    bool m_hw_render_enabled;
    struct retro_hw_render_callback m_hw_render_callback = {};

    // --- DEFINIÇÕES DE TIPOS (A ordem importa!) ---
    typedef void (*retro_init_t)(void);
    typedef void (*retro_deinit_t)(void);
    typedef unsigned (*retro_api_version_t)(void);
    typedef void (*retro_set_environment_t)(bool (*)(unsigned, void*));
    typedef void (*retro_set_video_refresh_t)(void (*)(const void*, unsigned, unsigned, size_t));
    typedef void (*retro_set_audio_sample_batch_t)(size_t(*)(const int16_t*, size_t));
    typedef void (*retro_set_input_poll_t)(void (*)(void));
    typedef void (*retro_set_input_state_t)(int16_t(*)(unsigned, unsigned, unsigned, unsigned));
    typedef bool (*retro_load_game_t)(struct retro_game_info* info);
    typedef void (*retro_run_t)(void);

    // Definindo o tipo AQUI, antes de usar:
    typedef void (*retro_set_controller_port_device_t)(unsigned port, unsigned device);

    // --- TYPEDEFS DE MEMÓRIA ---
    typedef void* (*retro_get_memory_data_t)(unsigned id);
    typedef size_t(*retro_get_memory_size_t)(unsigned id);

    retro_get_memory_data_t m_retro_get_memory_data = nullptr;
    retro_get_memory_size_t m_retro_get_memory_size = nullptr;

    // Variáveis Membro
    retro_init_t m_retro_init;
    retro_deinit_t m_retro_deinit;
    retro_load_game_t m_retro_load_game;
    retro_run_t m_retro_run;
    retro_set_environment_t m_retro_set_environment;

    // Agora o compilador reconhece o tipo:
    retro_set_controller_port_device_t m_retro_set_controller_port_device;

    // Assinatura das funções de DLL
    typedef size_t(*retro_serialize_size_t)(void);
    typedef bool   (*retro_serialize_t)(void* data, size_t size);
    typedef bool   (*retro_unserialize_t)(const void* data, size_t size);

    // Os ponteiros que vamos carregar
    retro_serialize_size_t m_retro_serialize_size;
    retro_serialize_t      m_retro_serialize;
    retro_unserialize_t    m_retro_unserialize;
};