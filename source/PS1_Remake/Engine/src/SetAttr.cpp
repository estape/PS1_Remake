#include "../include/SetAttr.h"
#include <utility>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

SDL_Gamepad* _gamepad = nullptr;

// --- SISTEMA DE DATABASE E SCANNER ---
std::unordered_map<std::string, std::string> g_gameDatabase;

void SetAttr::LoadGameDatabase(const std::string& csvFilePath) {
    std::ifstream file(csvFilePath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        size_t commaPos = line.find(',');
        if (commaPos != std::string::npos) {
            std::string id = line.substr(0, commaPos);
            std::string name = line.substr(commaPos + 1);
            id.erase(id.find_last_not_of(" \n\r\t") + 1);
            name.erase(name.find_last_not_of(" \n\r\t") + 1);
            g_gameDatabase[id] = name;
        }
    }
}

std::string SetAttr::ExtractPS1GameID(const std::string& binPath) {
    std::ifstream file(binPath, std::ios::binary);
    if (!file.is_open()) return "";

    const size_t CHUNK_SIZE = 1024 * 1024; // Lê de 1 em 1 Megabyte
    std::vector<char> buffer(CHUNK_SIZE);
    std::string anchor = "cdrom:\\";

    while (file) {
        std::streampos currentPos = file.tellg();
        file.read(buffer.data(), CHUNK_SIZE);
        size_t bytesRead = file.gcount();
        if (bytesRead == 0) break;

        auto it = std::search(buffer.begin(), buffer.begin() + bytesRead, anchor.begin(), anchor.end());

        if (it != buffer.begin() + bytesRead && std::distance(buffer.begin(), it) + anchor.length() + 11 <= bytesRead) {
            std::string rawID(it + anchor.length(), it + anchor.length() + 11);
            std::string cleanID = "";
            for (char c : rawID) {
                if (c == '_') cleanID += '-';
                else if (c != '.' && c != ';') cleanID += c;
            }
            return cleanID;
        }
        if (file) file.seekg(currentPos + (std::streampos)(CHUNK_SIZE - anchor.length() - 15));
    }
    return "";
}

int SetAttr::InitSDL3()
{
    //Inicializa SDL com suporte a Vídeo e Joystick
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        SDL_Log("Erro ao iniciar SDL: %s", SDL_GetError());
        return -1;
    }

	return 0;
}

void SetAttr::SetOpenGL3_3()
{
    //[IMPORTANTE] Configura o OpenGL 3.3 Core (Requisito do Beetle HW)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

SDL_Gamepad* SetAttr::GetGamepads(SDL_JoystickID* SDL_Joystick, int num)
{
    if (SDL_Joystick && num > 0) {
        // Abre o primeiro controle que achar
        _gamepad = SDL_OpenGamepad(SDL_Joystick[0]);
        if (_gamepad) {
            SDL_Log("Controle conectado: %s", SDL_GetGamepadName(_gamepad));
            return _gamepad;
        }
        return _gamepad;
    }
    return _gamepad;
}