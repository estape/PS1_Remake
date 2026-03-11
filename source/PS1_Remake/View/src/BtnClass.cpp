#include "../include/BtnClass.h"
#include <iostream>

BtnClass::BtnClass(SDL_Renderer* renderer, const std::string& imagePath, const std::string& text, TTF_Font* font, float x, float y, float w, float h)
    : backgroundTex(nullptr), textTex(nullptr), textRect{0.0f, 0.0f, 0.0f, 0.0f}
{
    buttonRect = { x, y, w, h };

    // ==========================================================
    // 1. CARREGA A IMAGEM DE FUNDO DO BOTÃO (Se houver)
    // ==========================================================
    if (!imagePath.empty()) {
        backgroundTex = IMG_LoadTexture(renderer, imagePath.c_str());
        if (!backgroundTex) {
            std::cout << "[UI ERRO] Falha ao carregar imagem do botao (" << imagePath << "): " << SDL_GetError() << std::endl;
        }
    }

    // ==========================================================
    // 2. GERA A TEXTURA DO TEXTO COM A FONTE (Se houver)
    // ==========================================================
    if (!text.empty() && font != nullptr) {
        // Cor do texto: Branco puro
        SDL_Color textColor = { 255, 255, 255, 255 }; 
        
        // Renderiza o texto numa Surface (Memória RAM)
        SDL_Surface* textSurface = TTF_RenderText_Blended(font, text.c_str(), 0, textColor);
        
        if (textSurface) {
            // Converte a Surface para Texture (Memória de Vídeo - VRAM)
            textTex = SDL_CreateTextureFromSurface(renderer, textSurface);
            
            // Matemática para centralizar o texto cirurgicamente no meio do botão
            textRect.w = static_cast<float>(textSurface->w);
            textRect.h = static_cast<float>(textSurface->h);
            textRect.x = buttonRect.x + (buttonRect.w - textRect.w) / 2.0f;
            textRect.y = buttonRect.y + (buttonRect.h - textRect.h) / 2.0f;

            // Limpa a Surface da RAM pois já temos a Textura na VRAM
            SDL_DestroySurface(textSurface);
        } else {
            std::cout << "[UI ERRO] Falha ao renderizar texto do botao (" << text << "): " << SDL_GetError() << std::endl;
        }
    }
}

BtnClass::~BtnClass() {
    // Regra de ouro do C++ de Baixo Nível: Você alocou? Você destrói.
    if (backgroundTex) SDL_DestroyTexture(backgroundTex);
    if (textTex) SDL_DestroyTexture(textTex);
}

void BtnClass::Render(SDL_Renderer* renderer) {
    // Desenha primeiro o fundo...
    if (backgroundTex) {
        SDL_RenderTexture(renderer, backgroundTex, nullptr, &buttonRect);
    }
    // ...e depois "carimba" o texto por cima!
    if (textTex) {
        SDL_RenderTexture(renderer, textTex, nullptr, &textRect);
    }
}

bool BtnClass::IsHovered(float mouseX, float mouseY) {
    // Colisão AABB simples (Verifica se o ponto X/Y do mouse está dentro do retângulo)
    return (mouseX >= buttonRect.x && mouseX <= (buttonRect.x + buttonRect.w) &&
            mouseY >= buttonRect.y && mouseY <= (buttonRect.y + buttonRect.h));
}

bool BtnClass::IsClicked(float mouseX, float mouseY, bool isMouseDown) {
    // Só é um clique válido se o mouse estiver por cima E o botão esquerdo estiver pressionado
    return IsHovered(mouseX, mouseY) && isMouseDown;
}