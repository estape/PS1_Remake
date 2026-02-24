void CreateMemoryCard(); // Criar dados de Memory Card formatado e em branco
void LoadIcon(); // Faz a leitura de dados de icone mais cores e desenha o icone
void WriteRawData(); // Escreve os dados RAW do Memory Card em .mcr
void ReadRawData(); // Lê os dados RAW do Memory Card
void WriteOnMemoryCard(); // Grava todos os dados RAW formatado em um Memory Card real (via Arduino).
void LoadOnMemoryCard(); // Lê todos os dados RAW de um Memory Card real (via Arduino).
void SaveMemoryCardData(); // Salva em JSON cada elemento do Memory Card em formato .smrc (Special Format Memory Card)
void LoadMemoryCardData(); // Carrega em JSON cada elemento do Memory Card do formato .smrc (Special Format Memory Card)