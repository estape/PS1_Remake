#include "../include/MMC_Handle.h"

std::vector<uint8_t> void CreateNewMemoryCard()
{
    // 1. Cria um array de 128KB (131072 bytes) zerado
    std::vector<uint8_t> mcData(131072, 0x00);

    // ==========================================================
    // FRAME 0: CABEÇALHO DO CARTÃO (Mágica da Sony)
    // ==========================================================
    mcData[0] = 'M'; // 0x4D
    mcData[1] = 'C'; // 0x43
    // O byte 127 é o Checksum (XOR dos dados anteriores). 0x4D ^ 0x43 = 0x0E
    mcData[127] = 0x0E; 

    // ==========================================================
    // FRAMES 1 a 15: DIRETÓRIO DE BLOCOS (Os 15 slots de save)
    // ==========================================================
    for (int i = 1; i <= 15; i++) {
        int offset = i * 128; // Cada frame tem 128 bytes
        
        mcData[offset] = 0xA0; // 0xA0 = Bloco Livre / Formatado
        
        // Em um bloco livre, o ponteiro de "próximo bloco" aponta para o infinito (0xFFFF)
        mcData[offset + 8] = 0xFF; 
        mcData[offset + 9] = 0xFF;
        
        // O Checksum deste frame (0xA0 ^ 0xFF ^ 0xFF = 0xA0)
        mcData[offset + 127] = 0xA0;
    }
    return mcData;
}

void LoadIcon()
{

}

void WriteRawData()
{
    // ==========================================================
    // GRAVAÇÃO NO DISCO FÍSICO DO PC
    // ==========================================================
    std::ofstream outFile(filepath, std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(mcData.data()), mcData.size());
        outFile.close();
        std::cout << "[MEMORY CARD] Cartao formatado criado com sucesso: " << filepath << std::endl;
    } else {
        std::cout << "[ERRO] Nao foi possivel criar o cartao: " << filepath << std::endl;
    }
}

void ReadRawData()
{

}

void WriteOnMemoryCard()
{

}

void LoadOnMemoryCard()
{

}

void SaveMemoryCardData()
{

}

void LoadMemoryCardData()
{

}