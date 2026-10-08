# Copilot Instructions

## Diretrizes de projeto
- Preferência do desenvolvedor: não converter string para enum; pretende usar um `enum` diretamente no `switch` para dispatch de comandos.
- Preferência para que `GAME_PATH` e `MemoryCardPath` permaneçam declaradas como `const` e permite alterar a função `SetGameAndMMCPath` para compatibilidade com o emulador.