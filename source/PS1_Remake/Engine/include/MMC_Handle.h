#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#pragma warning(push)
#pragma warning(disable : 26819) // Para eliminar os avisos do VS Community de acordo com a intenção do autor da biblioteca com o código.
#include "../include/json.hpp"
#pragma warning(pop)

#ifdef _WIN32
#include <windows.h>
#endif


// Estrutura padrão de 32-bits (RGBA) para o SDL3 e OpenGL
struct FLinearColor8 {
	uint8_t R, G, B, A;
};

typedef void* (*retro_get_memory_data_t)(unsigned id);
typedef size_t(*retro_get_memory_size_t)(unsigned id);

class MMC_Handle
{
public:
	static retro_get_memory_data_t get_memory_data_func;
	static retro_get_memory_size_t get_memory_size_func;

	std::vector<std::uint8_t> CreateNewMemoryCard(const std::string& filepath); // Cria um novo Memory Card formatado com 15 slots vazios e salva em .mcr
	static std::vector<FLinearColor8> LoadIcon(int slotNumber, const std::vector<uint8_t>& mcData); // Faz a leitura de dados de icone mais cores e desenha o icone, passamos o buffer de 128KB e o número do slot (0 a 14)
	void WriteRawData(const std::string& filepath, const std::vector<std::uint8_t>& mcData); // Escreve os dados RAW do Memory Card em .mcr
	void ReadRawData(const std::string& filepath); // Lê os dados RAW do Memory Card
	void WriteFromMemoryCard(int slotNumber, const std::string& importPath); // Grava todos os dados RAW formatado em um Memory Card real (via Arduino).
	void LoadFromMemoryCard(int slotNumber, const std::string& exportPath); // Lê todos os dados RAW de um Memory Card real (via Arduino).
	void SaveSpecialMemoryCard(const std::string& filepath); // Salva em JSON cada elemento do Memory Card em formato .smrc (Special Format Memory Card)
	void LoadSpecialMemoryCard(const std::string& filepath); // Carrega em JSON cada elemento do Memory Card do formato .smrc (Special Format Memory Card)
	static std::string GetSaveTitle(int slotNumber, const std::vector<uint8_t>& mcData); // Lê o título do save (Os primeiros 16 bytes do bloco de save) e converte de SHIFT-JIS para UTF-8 usando a função auxiliar ToFirstUpper

private:
	static std::string ToFirstUpper(std::string InString); // O seu helper de formatação para SHIFT-JIS
};