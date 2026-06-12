/*
 * ============================================================================
 *  tipos.h  -  Definicoes compartilhadas do projeto ChargeGrid SmartHub
 * ============================================================================
 *
 *  Este arquivo de cabecalho ("header") declara todos os TIPOS DE DADOS que
 *  serao usados pelos outros modulos do sistema. Ele nao contem logica - apenas
 *  definicoes que os outros .c precisam "enxergar" para se entenderem entre si.
 *
 *  Por que separar isso aqui? Porque o struct Sessao e usado pelo modulo
 *  sessao.c, pelo demanda.c, pelo tarifa.c, pelo ocpp.c e pelo relatorio.c.
 *  Se cada um redefinisse a struct, teriamos cinco copias divergentes. Com
 *  tipos.h, ha uma fonte unica de verdade.
 *
 *  EV Challenge 2026 - FIAP x GoodWe
 *  Disciplina: Data Structure and Algorithms
 * ============================================================================
 */

/*
 * Include guard - impede que este header seja incluido duas vezes no mesmo
 * arquivo (o que causaria erro de "tipo redefinido" no compilador).
 *
 * Padrao: a primeira vez que o pre-processador ve este arquivo, define a
 * macro TIPOS_H. Da segunda em diante, o #ifndef pula o conteudo inteiro.
 */
#ifndef TIPOS_H
#define TIPOS_H

/* ===========================================================================
 *  CONSTANTES DO SISTEMA
 * ===========================================================================
 *
 *  Usamos #define para criar "apelidos" para numeros importantes. Vantagem:
 *  se um dia precisarmos aumentar o limite de sessoes, mudamos so aqui.
 */

#define MAX_SESSOES         20      /* numero maximo de sessoes simultaneas    */
#define DEMANDA_CONTRATADA  75.0f   /* potencia maxima do edificio (kW)        */
#define MIN_POTENCIA_CARGA  1.4f    /* limiar IEC 61851 - abaixo disso pausa   */
#define TAMANHO_PLACA       8       /* "ABC1D23" + terminador nulo             */
#define TAMANHO_ID_CARREG   16      /* identificador do carregador             */
#define TAMANHO_ID_MENSAGEM 36      /* messageId no padrao OCPP                */
#define TARIFA_BASE         0.85f   /* R$/kWh em condicao normal               */


/* ===========================================================================
 *  ENUMERACOES (enum) - listas de valores nomeados
 * ===========================================================================
 *
 *  Um enum e um tipo cujo valor so pode ser um dos rotulos listados. Em vez
 *  de usar int 0, 1, 2, 3, 4 (que ninguem lembra o que significa), usamos
 *  nomes legiveis. O compilador converte para int internamente.
 */

/*
 * Estados possiveis de uma sessao de recarga.
 * Modela o ciclo de vida real: ocioso -> aguardando autorizacao -> carregando
 * -> (talvez pausado por demanda) -> finalizada. ERRO e estado terminal.
 */
typedef enum {
    OCIOSO,
    AGUARDANDO_AUTORIZACAO,
    CARREGANDO,
    PAUSADA_DEMANDA,
    FINALIZADA,
    ERRO
} EstadoSessao;

/*
 * Modelos da linha HCA G2 da GoodWe (eletropostos comerciais).
 * Cada modelo tem uma potencia nominal diferente, definida em
 * sessao.c na funcao potencia_do_modelo().
 */
typedef enum {
    GW7K,    /* 7 kW  - vagas de longa permanencia (escritorios, shoppings)   */
    GW11K,   /* 11 kW - vagas mistas (supermercados, frota leve)              */
    GW22K    /* 22 kW - vagas de fluxo rapido (postos, frota corporativa)     */
} ModeloHCA;


/* ===========================================================================
 *  STRUCT Sessao - registro de uma sessao de recarga
 * ===========================================================================
 *
 *  Um struct agrupa varios campos relacionados num unico tipo. E como criar
 *  um "molde": depois disso, podemos declarar variaveis do tipo Sessao e
 *  cada uma carrega todos esses campos.
 *
 *  O typedef no final faz com que possamos escrever Sessao em vez de
 *  struct Sessao - menos verboso, ja que vamos usar muito.
 */
typedef struct {
    int           id;                            /* identificador unico (1, 2, 3...) */
    char          placa_veiculo[TAMANHO_PLACA];  /* ex: "ABC1D23"                    */
    char          id_carregador[TAMANHO_ID_CARREG]; /* ex: "CHG-SHP-01"              */
    ModeloHCA     modelo;                        /* modelo do carregador              */
    EstadoSessao  estado;                        /* situacao atual no ciclo de vida   */
    int           hora_inicio;                   /* hora simulada 0-23                */
    int           minuto_inicio;                 /* minuto simulado 0-59              */
    float         potencia_nominal_kw;           /* capacidade do carregador          */
    float         potencia_atual_kw;             /* potencia entregue agora           */
    float         kwh_entregue;                  /* energia ja entregue na sessao     */
    float         custo_acumulado;               /* valor em R$                       */
    int           prioridade;                    /* 1=normal, 2=frota, 3=emergencia   */
} Sessao;


/* ===========================================================================
 *  FIM DO HEADER
 * ===========================================================================
 */
#endif /* TIPOS_H */
