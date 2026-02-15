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

enum retro_log_level {
    RETRO_LOG_DEBUG = 0,
    RETRO_LOG_INFO,
    RETRO_LOG_WARN,
    RETRO_LOG_ERROR,
    RETRO_LOG_DUMMY = 2147483647
};

enum retro_hw_context_type {
    RETRO_HW_CONTEXT_NONE = 0,
    RETRO_HW_CONTEXT_OPENGL = 1,
    RETRO_HW_CONTEXT_OPENGLES2 = 2,
    RETRO_HW_CONTEXT_OPENGL_CORE = 3,
    RETRO_HW_CONTEXT_VULKAN = 6
};

struct retro_log_callback {
    void (*log)(enum retro_log_level level, const char* fmt, ...);
};

struct retro_hw_render_callback {
    enum retro_hw_context_type context_type;
    unsigned version_major;
    unsigned version_minor;
    void (*context_reset)(void);
    void (*context_destroy)(void);
    bool cache_context;
    unsigned depth;
    bool stencil;
    unsigned bottom_left_origin;
    unsigned version_major_backend;
    unsigned version_minor_backend;
};

struct retro_game_info {
    const char* path;
    const void* data;
    size_t size;
    const char* meta;
};

class CoreWrapper {
public:
    CoreWrapper();
    ~CoreWrapper();

    bool LoadCore(const std::string& dllPath);
    bool LoadGame(const std::string& gamePath);
    void RunFrame();
    void Unload();
    void EnableHardwareRenderer();
    void InitVideo(SDL_Renderer* renderer);

    static bool EnvironmentCallback(unsigned cmd, void* data);
    static void VideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch);
    static void InputPoll();
    static int16_t InputState(unsigned port, unsigned device, unsigned index, unsigned id);
    static size_t AudioSampleBatch(const int16_t* data, size_t frames);
    static void OnContextReset();
    static void OnContextDestroy();

    bool SaveState(const std::string& filepath);
    bool LoadState(const std::string& filepath);

private:
    void* m_coreHandle;
    bool m_hw_render_enabled;

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

    // [IMPORTANTE] Definindo o tipo AQUI, antes de usar:
    typedef void (*retro_set_controller_port_device_t)(unsigned port, unsigned device);

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