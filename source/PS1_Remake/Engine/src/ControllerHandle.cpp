#include "../include/ControllerHandle.h"

// --- VARIÁVEIS REAIS (Moram exclusivamente aqui!) ---
// A variável que guarda o ponteiro da DLL
global_set_port_t g_set_controller_func = nullptr;

static unsigned s_currentDeviceId = RETRO_DEVICE_PS_DIGITAL;
static bool s_btnTouchpadLastState = false;
static SDL_Gamepad* s_activeGamepad = nullptr;

static uint16_t s_rumble_strong = 0;
static uint16_t s_rumble_weak = 0;
static bool s_rumble_enabled = true;

// ---------------------------------------------------------

void ControllerHandle::InputPoll() {
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

    // --- LÓGICA DO TOUCHPAD (Troca Modo Digital/Modo analógico) ---
    bool isPressed = SDL_GetGamepadButton(s_activeGamepad, SDL_GAMEPAD_BUTTON_TOUCHPAD);

    // Alterna entre os modos digital e analógico apenas quando aperta (Borda de subida)
    if (isPressed && !s_btnTouchpadLastState) {
        std::cout << "[CONTROLE] Touchpad pressionado!" << std::endl;

        // Inverte: 1 -> 5 ou 5 -> 1
        s_currentDeviceId = (s_currentDeviceId == RETRO_DEVICE_PS_DIGITAL)
            ? RETRO_DEVICE_PS_DUALSHOCK
            : RETRO_DEVICE_PS_DIGITAL;

        if (g_set_controller_func) {
            std::cout << "[CONTROLE] Ponteiro VALIDO! Mudando para modo: " << s_currentDeviceId << std::endl;
            g_set_controller_func(0, s_currentDeviceId);
        }
        else {
            std::cout << "[ERRO] O ponteiro da DLL esta NULO no ControllerHandle!" << std::endl;
        }
    }

    s_btnTouchpadLastState = isPressed;
}

void ControllerHandle::ToggleRumble() {
    s_rumble_enabled = !s_rumble_enabled;
    std::cout << "[CONTROLE] Vibracao: " << (s_rumble_enabled ? "LIGADA" : "DESLIGADA") << std::endl;
}

bool ControllerHandle::SetRumbleState(unsigned port, enum retro_rumble_effect effect, uint16_t strength) {
    if (port != 0 || !s_activeGamepad) return false;

    if (!s_rumble_enabled) {
        SDL_RumbleGamepad(s_activeGamepad, 0, 0, 0);
        return true;
    }

    if (effect == RETRO_RUMBLE_STRONG) {
        s_rumble_strong = strength;
    }
    else if (effect == RETRO_RUMBLE_WEAK) {
        s_rumble_weak = strength;
    }

    SDL_RumbleGamepad(s_activeGamepad, s_rumble_strong, s_rumble_weak, 0xFFFF);
    return true;
}

int16_t ControllerHandle::InputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || !s_activeGamepad) return 0;

    if (device == RETRO_DEVICE_ANALOG || device == RETRO_DEVICE_PS_DUALSHOCK) {
        if (s_currentDeviceId != RETRO_DEVICE_PS_DUALSHOCK) return 0;

        SDL_GamepadAxis targetAxis;
        float deadzone, saturation;

        if (index == 0) {
            targetAxis = (id == 0) ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_LEFTY;
            deadzone = 4000.0f;
            saturation = 24000.0f;
        }
        else {
            targetAxis = (id == 0) ? SDL_GAMEPAD_AXIS_RIGHTX : SDL_GAMEPAD_AXIS_RIGHTY;
            deadzone = 7000.0f;
            saturation = 26000.0f;
        }

        int rawVal = SDL_GetGamepadAxis(s_activeGamepad, targetAxis);
        float val = (float)rawVal;
        float absVal = std::abs(val);

        if (absVal < deadzone) return 0;

        if (absVal >= saturation) {
            return (val > 0) ? 32700 : -32700;
        }

        float normalized = (absVal - deadzone) / (saturation - deadzone);
        float finalVal = normalized * 32700.0f;

        if (val < 0) finalVal = -finalVal;

        return (int16_t)finalVal;
    }

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