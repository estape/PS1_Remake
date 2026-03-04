#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>

class BtnClass {
public:
    // Construtor: Recebe o Renderizador, caminho da imagem, texto, fonte e o retângulo (X, Y, Largura, Altura)
    BtnClass(SDL_Renderer* renderer, const std::string& imagePath, const std::string& text, TTF_Font* font, float x, float y, float w, float h);
    
    // Destrutor: Limpa as texturas da placa de vídeo quando a tela for fechada
    ~BtnClass();

    // Desenha o botão na tela
    void Render(SDL_Renderer* renderer);

    // Funções de colisão do mouse
    bool IsHovered(float mouseX, float mouseY);
    bool IsClicked(float mouseX, float mouseY, bool isMouseDown);

private:
    SDL_Texture* backgroundTex; // Textura da imagem de fundo (ex: o arco-íris do PS1)
    SDL_Texture* textTex;       // Textura gerada a partir da fonte StoneSans
    
    SDL_FRect buttonRect;       // A caixa de colisão geométrica do botão
    SDL_FRect textRect;         // A caixa que vai centralizar o texto no meio do botão
};