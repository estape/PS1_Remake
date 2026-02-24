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

#define RETRO_DEVICE_JOYPAD     1
#define RETRO_DEVICE_ANALOG     5
#define RETRO_DEVICE_PS_DIGITAL   1
#define RETRO_DEVICE_PS_DUALSHOCK 517 
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

enum retro_rumble_effect {
    RETRO_RUMBLE_STRONG = 0,
    RETRO_RUMBLE_WEAK = 1
};

struct retro_rumble_interface {
    bool (*set_rumble_state)(unsigned port, enum retro_rumble_effect effect, uint16_t strength);
};

// EXTERN: A promessa de que a variável existe no .cpp!
typedef void (*global_set_port_t)(unsigned, unsigned);
extern global_set_port_t g_set_controller_func;

class ControllerHandle {
public:
    static void InputPoll();
    static int16_t InputState(unsigned port, unsigned device, unsigned index, unsigned id);
    static bool SetRumbleState(unsigned port, enum retro_rumble_effect effect, uint16_t strength);
    void ToggleRumble();
};