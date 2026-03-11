#include "../include/MMC_Handle.h"

using json = nlohmann::json;

retro_get_memory_data_t MMC_Handle::get_memory_data_func = nullptr;
retro_get_memory_size_t MMC_Handle::get_memory_size_func = nullptr;

std::vector<std::uint8_t> MMC_Handle::CreateNewMemoryCard(const std::string& filepath)
{
    // 1. Cria um array de 128KB (131072 bytes) zerado
    std::vector<std::uint8_t> mcData(131072, 0x00);

    // ==========================================================
    // FRAME 0: CABEÇALHO DO CARTÃO (Mágica da Sony)
    // ==========================================================
    mcData[0] = static_cast<std::uint8_t>('M'); // 0x4D
    mcData[1] = static_cast<std::uint8_t>('C'); // 0x43
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
    // Escreve o arquivo no disco usando a função auxiliar
    WriteRawData(filepath, mcData);

    return mcData;
}

std::vector<FLinearColor8> MMC_Handle::LoadIcon(int slotNumber, const std::vector<uint8_t>& mcData) {
    // Retorna um ícone vazio e transparente se o slot for inválido
    if (slotNumber < 0 || slotNumber > 14 || mcData.size() < 131072) {
        return std::vector<FLinearColor8>(256, { 0, 0, 0, 0 });
    }

    std::vector<FLinearColor8> palette(16);
    std::vector<FLinearColor8> iconPixels(256);

    // O offset base do slot de save escolhido (ignorando o bloco 0 do diretório)
    int blockOffset = 8192 + (slotNumber * 8192);

    // =========================================================
    // ETAPA 1: LER A PALETA DE CORES (CLUT)
    // Offset 0x60 (96) dentro do bloco.
    // =========================================================
    int clutOffset = blockOffset + 96;

    for (int i = 0; i < 16; i++) {
        // Agrupar os 16-bits em Little-Endian
        uint16_t colorData = (mcData[clutOffset + (i * 2) + 1] << 8) | mcData[clutOffset + (i * 2)];

        // Extração dos canais RGB555 convertidos para 8-bits (0-255)
        uint8_t R = (colorData & 0x1F) << 3;
        uint8_t G = ((colorData >> 5) & 0x1F) << 3;
        uint8_t B = ((colorData >> 10) & 0x1F) << 3;

        // Regra oficial de Transparência da Sony (Bit 15 / STP)
        uint8_t A = (colorData == 0x0000) ? 0 : 255;

        palette[i] = { R, G, B, A };
    }

    // =========================================================
    // ETAPA 2: LER OS PIXELS (Formato 4-bits)
    // Offset 0x80 (128) dentro do bloco. 128 bytes no total.
    // =========================================================
    int pixelOffset = blockOffset + 128;
    int pixelIndex = 0;

    for (int i = 0; i < 128; i++) {
        uint8_t data = mcData[pixelOffset + i];

        // No PS1, o "nibble" (4-bits) da DIREITA é o primeiro pixel a ser desenhado.
        uint8_t p1_index = data & 0x0F;
        // O "nibble" da ESQUERDA é o segundo pixel.
        uint8_t p2_index = (data >> 4) & 0x0F;

        // Pinta os dois pixels na nossa matriz final usando as cores da paleta
        iconPixels[pixelIndex++] = palette[p1_index];
        iconPixels[pixelIndex++] = palette[p2_index];
    }

    return iconPixels;
}

void MMC_Handle::WriteRawData(const std::string& filepath, const std::vector<std::uint8_t>& mcData)
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

// ==========================================================
// LEITURA DO ARQUIVO BINÁRIO DIRETO PARA A RAM DO EMULADOR
// ==========================================================
void MMC_Handle::ReadRawData(const std::string& filepath)
{
    if (!get_memory_data_func || !get_memory_size_func) return;

    size_t mcSize = get_memory_size_func(0);
    uint8_t* mcData = (uint8_t*)get_memory_data_func(0);

    if (mcSize == 0 || mcData == nullptr) return;

    std::ifstream inFile(filepath, std::ios::binary);
    if (inFile.is_open()) {
        // Pega os 128KB do arquivo .mcr e joga inteiro na RAM do emulador de uma vez
        inFile.read(reinterpret_cast<char*>(mcData), mcSize);
        inFile.close();
        std::cout << "[MEMORY CARD] Arquivo carregado na RAM com sucesso: " << filepath << std::endl;
    }
    else {
        // A Mágica: Se o jogo pedir o Memory Card e o arquivo não existir, criamos um novo!
        std::cout << "[MEMORY CARD] Arquivo nao encontrado. Gerando novo cartao..." << std::endl;
        std::vector<uint8_t> newData = CreateNewMemoryCard(filepath);

        // Copia a matriz gerada (newData) direto para o ponteiro da RAM (mcData)
        std::memcpy(mcData, newData.data(), mcSize);
    }
}

// Injeta um arquivo ".mcs" (Single Save) num slot específico
void MMC_Handle::WriteFromMemoryCard(int slotNumber, const std::string& importPath)
{
    if (!get_memory_data_func) return;
    uint8_t* mcData = (uint8_t*)get_memory_data_func(0);

    // (Futuro) Aqui você vai abrir o save individual baixado da internet
    // e injetar exatamente nos offsets do slotNumber escolhido.
    std::cout << "[MEMCARD MGR] Injetando save externo no Slot " << slotNumber << "..." << std::endl;
}

// Extrai apenas 1 bloco (8KB) + Seu cabeçalho do diretório (128 bytes)
void MMC_Handle::LoadFromMemoryCard(int slotNumber, const std::string& exportPath)
{
    if (!get_memory_data_func) return;
    uint8_t* mcData = (uint8_t*)get_memory_data_func(0);

    // Calcula onde esse save está na RAM
    int directoryOffset = slotNumber * 128; // Onde fica o nome/link do save
    int blockOffset = 8192 + (slotNumber * 8192); // Onde ficam os dados/ícone

    // (Futuro) Aqui você vai copiar esses dados para um arquivo ".mcs" (Single Save)
    std::cout << "[MEMCARD MGR] Extraindo save do Slot " << slotNumber << "..." << std::endl;
}

void MMC_Handle::SaveSpecialMemoryCard(const std::string& filepath) {
    if (!get_memory_data_func || !get_memory_size_func) return;

    size_t mcSize = get_memory_size_func(0);
    uint8_t* mcDataPtr = (uint8_t*)get_memory_data_func(0);

    // Converte o ponteiro cru para um std::vector seguro para passarmos para as nossas funções
    std::vector<uint8_t> mcData(mcDataPtr, mcDataPtr + mcSize);

    // =========================================================
    // 1. O NÓ "JSON Object" Mestre
    // =========================================================
    json smrc_json;
    smrc_json["HeaderAmount"] = 1;
    smrc_json["BlockAmount"] = 15;

    // =========================================================
    // 2. OS DADOS BRUTOS (Para backup e reescrita)
    // =========================================================
    std::vector<uint8_t> masterHeader(mcData.begin(), mcData.begin() + 128);
    smrc_json["Header"] = masterHeader;

    std::vector<uint8_t> gameHeaders(mcData.begin() + 128, mcData.begin() + 2048);
    smrc_json["GameHeader"] = gameHeaders;

    std::vector<uint8_t> rawSaveData(mcData.begin() + 8192, mcData.end());
    smrc_json["MC_Data"] = rawSaveData;

    // =========================================================
    // 3. OS METADADOS MASTIGADOS PARA A SUA UI LER (A Mágica!)
    // =========================================================
    json slotsArray = json::array();

    for (int i = 0; i < 15; i++) {
        json slotJson;
        slotJson["SlotNumber"] = i;

        // Chama a nossa função que decodifica o Shift-JIS
        std::string title = GetSaveTitle(i, mcData);
        slotJson["Title"] = title;

        // Se o slot não estiver vazio, extraímos os pixels do ícone!
        if (title != "Empty Block" && title != "Slot Invalido") {
            std::vector<FLinearColor8> pixels = LoadIcon(i, mcData);

            json iconArray = json::array();
            for (const auto& p : pixels) {
                // Salva cada pixel como uma array [R, G, B, A] para o SDL3 puxar fácil depois
                iconArray.push_back({ p.R, p.G, p.B, p.A });
            }
            slotJson["IconPixels"] = iconArray;
        }
        else {
            slotJson["IconPixels"] = nullptr; // Slot vazio não tem ícone
        }

        slotsArray.push_back(slotJson);
    }

    smrc_json["Slots"] = slotsArray;

    // =========================================================
    // 4. SALVANDO O ARQUIVO NO DISCO
    // =========================================================
    std::ofstream outFile(filepath);
    if (outFile.is_open()) {
        outFile << smrc_json.dump(4);
        outFile.close();
        std::cout << "[JSON SMRC] Gerenciador criado com sucesso em: " << filepath << std::endl;
    }
}

void MMC_Handle::LoadSpecialMemoryCard(const std::string& filepath)
{
    std::ifstream inFile(filepath);
    if (inFile.is_open()) {
        json smrc_json;
        // Transforma o texto puro do arquivo de volta num Objeto JSON estruturado
        inFile >> smrc_json;
        inFile.close();

        std::cout << "[JSON SMRC] Gerenciador lido com sucesso! Blocos totais: "
            << smrc_json["BlockAmount"] << std::endl;

        // Como o smrc_json está em memória agora, a sua UI poderá puxar 
        // smrc_json["Slots"][0]["Title"] para imprimir os textos na tela!
    }
    else {
        std::cout << "[ERRO] Arquivo SMRC nao encontrado para leitura: " << filepath << std::endl;
    }
}

// ==========================================================
// EXTRAÇÃO E TRADUÇÃO DO TÍTULO DO JOGO (Shift-JIS -> UTF-8)
// ==========================================================
std::string MMC_Handle::GetSaveTitle(int slotNumber, const std::vector<uint8_t>& mcData) {
    if (slotNumber < 0 || slotNumber > 14 || mcData.size() < 131072) return "Slot Invalido";

    // O título fica bem no comecinho do bloco (Offset 0) e tem no máximo 64 bytes
    int blockOffset = 8192 + (slotNumber * 8192);

    // Descobre o tamanho real do título procurando o byte nulo (0x00) que indica o fim da frase
    int titleLength = 0;
    while (titleLength < 64 && mcData[blockOffset + titleLength] != 0x00) {
        titleLength++;
    }

    if (titleLength == 0) return "Empty Block";

#ifdef _WIN32
    // 1. Shift-JIS (CP 932) para UTF-16 (O seu node do Blueprint!)
    int utf16Length = MultiByteToWideChar(932, 0, (const char*)&mcData[blockOffset], titleLength, nullptr, 0);
    std::wstring utf16Str(utf16Length, 0);
    MultiByteToWideChar(932, 0, (const char*)&mcData[blockOffset], titleLength, &utf16Str[0], utf16Length);

    // 2. UTF-16 para UTF-8 (A conversão vital para o SDL3 e pro nosso .smrc)
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), utf16Length, nullptr, 0, nullptr, nullptr);
    std::string utf8Str(utf8Length, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), utf16Length, &utf8Str[0], utf8Length, nullptr, nullptr);

    return ToFirstUpper(utf8Str);
#else
    // Fallback para compilação em Linux (Copia os bytes diretos como ASCII)
    std::string asciiStr((const char*)&mcData[blockOffset], titleLength);
    return ToFirstUpper(asciiStr);
#endif
}

// ==========================================================
// FORMATAÇÃO DE STRING (Sua função ToFirstUpper traduzida)
// ==========================================================
std::string MMC_Handle::ToFirstUpper(std::string InString) {
    if (InString.empty()) return InString;

    // Transforma tudo em minúsculo
    std::transform(InString.begin(), InString.end(), InString.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Deixa a primeira letra maiúscula (O charme do design do PS1!)
    InString[0] = std::toupper(InString[0]);

    return InString;
}