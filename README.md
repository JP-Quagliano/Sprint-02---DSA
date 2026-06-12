# ChargeGrid SmartHub

Sistema em C para gerenciamento operacional de eletropostos comerciais GoodWe.
Sprint 2 da disciplina **Data Structure and Algorithms** — EV Challenge 2026, FIAP × GoodWe.

> Para a descrição completa da arquitetura, algoritmos, justificativas técnicas e mapeamento com a rubrica, consulte `docs/documento_tecnico.md`.

## Estrutura

```
.
├── .vscode/tasks.json      # configuracao de compilacao do VS Code
├── src/                    # codigo-fonte (6 modulos + main)
├── relatorios/             # destino dos arquivos .txt gerados pelo sistema
├── docs/
│   └── documento_tecnico.md
└── README.md
```

## Compilação

**Via VS Code:** abra a pasta do projeto e pressione `Ctrl+Shift+B`.

**Via terminal:**

```bash
gcc -Wall -Wextra -std=c11 -o chargegrid \
    src/main.c src/sessao.c src/demanda.c src/tarifa.c \
    src/ocpp.c src/relatorio.c
```

## Execução

```
.\chargegrid.exe       (Windows)
./chargegrid           (Linux/macOS)
```

## Funcionalidades

- Gerenciamento de até 20 sessões de recarga simultâneas, com máquina de 6 estados
- Controle inteligente de demanda com triagem por prioridade (emergência > frota > normal)
- Tarifação dinâmica multi-variável (horário × ocupação × modelo HCA)
- Simulação do protocolo OCPP 1.6-J (7 actions, formato JSON estruturado, request/response correlacionado)
- Geração de relatório operacional consolidado em arquivo `.txt`
- Menu interativo com validação de entrada e relógio simulado

## Pré-requisitos

- GCC (Linux/macOS nativo, ou MinGW-w64 no Windows via MSYS2)
- Visual Studio Code com a extensão C/C++ da Microsoft (opcional, mas recomendado)

## Integrantes

| Nome | RM |
|------|------|
| João Pedro Quagliano | 570233 |
| Leticia Aiko Okano | 571988 |
| Thiago Calazans Luz Nakano | 569151 |
| Enzo Scattolin Furtado | 570824 |
| Guilherme De Lucena Fontes | 569658 |
| Matheus Levi Dagel | 571961 |

## Licença

Trabalho acadêmico — distribuição livre para fins educacionais.
