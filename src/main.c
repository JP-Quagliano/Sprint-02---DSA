/*
 * ============================================================================
 *  main.c  -  Ponto de entrada e menu interativo
 *  ChargeGrid SmartHub - Sistema Inteligente de Gerenciamento de Recarga
 *  EV Challenge 2026 - FIAP x GoodWe
 * ============================================================================
 *
 *  Este arquivo apenas ORQUESTRA. Ele lê a escolha do operador no menu e
 *  chama as funcoes apropriadas em cada modulo. Toda a logica de verdade
 *  esta nos modulos (sessao.c, demanda.c, tarifa.c, ocpp.c, relatorio.c).
 *
 *  Esta e a versao da PARTE 1 da entrega. Algumas opcoes do menu mostram
 *  "[em construcao]" porque os modulos correspondentes ainda nao foram
 *  adicionados. Eles entrarao nas Partes 2, 3 e 4.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tipos.h"
#include "sessao.h"
#include "demanda.h"
#include "tarifa.h"
#include "ocpp.h"
#include "relatorio.h"


/* ===========================================================================
 *  Helpers de entrada do usuario
 * ===========================================================================
 *
 *  scanf "puro" e perigoso: se o usuario digita uma letra onde se esperava
 *  numero, o stdin trava num loop infinito. Essas funcoes auxiliares
 *  validam a entrada e limpam o buffer.
 */

/*
 * limpar_buffer  -  consome tudo que sobrou no stdin ate o proximo \n.
 * Necessario depois de scanf que le tipos numericos.
 */
static void limpar_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* descarta */
    }
}

/*
 * ler_inteiro  -  le um inteiro do stdin com mensagem customizada.
 * Repete a pergunta enquanto o usuario nao digitar um numero valido.
 */
static int ler_inteiro(const char *mensagem) {
    int valor;
    while (1) {
        printf("%s", mensagem);
        if (scanf("%d", &valor) == 1) {
            limpar_buffer();
            return valor;
        }
        printf("  [aviso] Entrada invalida. Digite um numero inteiro.\n");
        limpar_buffer();
    }
}

/*
 * ler_string  -  le uma string (sem espacos) do stdin no buffer fornecido.
 * O parametro 'tamanho' protege contra buffer overflow ao limitar o scanf.
 */
static void ler_string(const char *mensagem, char *buffer, int tamanho) {
    /* monta o formato dinamicamente: "%<tamanho-1>s" - ex: "%7s" para 8 bytes */
    char formato[16];
    snprintf(formato, sizeof(formato), "%%%ds", tamanho - 1);

    printf("%s", mensagem);
    if (scanf(formato, buffer) != 1) {
        buffer[0] = '\0';
    }
    limpar_buffer();
}


/* ===========================================================================
 *  Acoes do menu
 * ===========================================================================
 */

static void acao_nova_sessao(void) {
    char placa[TAMANHO_PLACA];
    int  escolha_modelo;
    int  hora, minuto, prioridade;

    printf("\n  >>> Nova sessao de recarga <<<\n");

    ler_string("    Placa do veiculo (ex: ABC1D23): ", placa, TAMANHO_PLACA);

    printf("    Modelo HCA G2:\n");
    printf("      1) GW7K-HCA-20   (7 kW)\n");
    printf("      2) GW11K-HCA-20  (11 kW)\n");
    printf("      3) GW22K-HCA-20  (22 kW)\n");
    escolha_modelo = ler_inteiro("    Escolha (1-3): ");

    ModeloHCA modelo;
    switch (escolha_modelo) {
        case 1:  modelo = GW7K;  break;
        case 2:  modelo = GW11K; break;
        case 3:  modelo = GW22K; break;
        default:
            printf("    [erro] Modelo invalido. Operacao cancelada.\n\n");
            return;
    }

    hora       = ler_inteiro("    Hora de inicio simulada (0-23): ");
    minuto     = ler_inteiro("    Minuto de inicio simulado (0-59): ");
    prioridade = ler_inteiro("    Prioridade (1=normal, 2=frota, 3=emergencia): ");

    sessao_criar(placa, modelo, hora, minuto, prioridade);

    /*
     *  Apos criar a sessao, o controle de demanda e acionado automaticamente.
     *  Se o pedido total ultrapassou DEMANDA_CONTRATADA, a alocacao e
     *  redistribuida e algumas sessoes podem entrar em PAUSADA_DEMANDA.
     */
    demanda_aplicar();
    printf("\n");
}


static void acao_encerrar_sessao(void) {
    printf("\n  >>> Encerrar sessao <<<\n");
    int id = ler_inteiro("    ID da sessao a encerrar: ");
    if (sessao_encerrar(id)) {
        /*
         *  Ao encerrar uma sessao, sobra "orcamento" de potencia. Outras
         *  sessoes que estavam em PAUSADA_DEMANDA podem voltar a carregar.
         */
        demanda_aplicar();
    }
    printf("\n");
}


static void acao_detalhe_sessao(void) {
    printf("\n  >>> Detalhe de sessao <<<\n");
    int id = ler_inteiro("    ID da sessao: ");
    sessao_imprimir_detalhe(id);
}


static void acao_avancar_tempo(void) {
    printf("\n  >>> Tempo simulado <<<\n");
    printf("    [1] Avancar N minutos a partir de agora\n");
    printf("    [2] Saltar diretamente para hora especifica HH:MM\n");
    int sub = ler_inteiro("    Escolha (1-2): ");

    if (sub == 1) {
        int min = ler_inteiro("    Minutos a avancar (1-720): ");
        if (min < 1 || min > 720) {
            printf("    [erro] Intervalo permitido: 1 a 720 minutos.\n\n");
            return;
        }
        sessao_avancar_tempo(min);

    } else if (sub == 2) {
        int h = ler_inteiro("    Nova hora (0-23): ");
        int m = ler_inteiro("    Novo minuto (0-59): ");
        if (h < 0 || h > 23 || m < 0 || m > 59) {
            printf("    [erro] Horario invalido.\n\n");
            return;
        }
        /*
         *  Calcula a quantidade de minutos a avancar para chegar no
         *  HH:MM pedido. Sempre avanca para frente - se o destino for
         *  "antes" do relogio atual, considera no proximo dia.
         */
        int agora_min = hora_simulada * 60 + minuto_simulada;
        int destino_min = h * 60 + m;
        int delta = destino_min - agora_min;
        if (delta <= 0) delta += 24 * 60;
        sessao_avancar_tempo(delta);

    } else {
        printf("    [aviso] Opcao invalida.\n\n");
        return;
    }
    printf("\n");
}


static void acao_controle_demanda(void) {
    printf("\n  >>> Controle de demanda - load balancing <<<\n");
    printf("    [1] Aplicar redistribuicao agora\n");
    printf("    [2] Apenas mostrar status (sem alterar)\n");
    int sub = ler_inteiro("    Escolha (1-2): ");

    if (sub == 1) {
        demanda_aplicar();
    } else if (sub == 2) {
        demanda_imprimir_status();
    } else {
        printf("    [aviso] Opcao invalida.\n\n");
    }
}


static void acao_tarifa(void) {
    printf("\n  >>> Tarifa dinamica <<<\n");
    printf("    [1] Mostrar decomposicao da tarifa atual\n");
    printf("    [2] Recalcular custos das sessoes ativas com a tarifa atual\n");
    int sub = ler_inteiro("    Escolha (1-2): ");

    if (sub == 1) {
        tarifa_explicar();
    } else if (sub == 2) {
        tarifa_recalcular_sessoes();
        printf("\n");
    } else {
        printf("    [aviso] Opcao invalida.\n\n");
    }
}


static void acao_ocpp_console(void) {
    printf("\n  >>> Console OCPP - mensagens trafegadas <<<\n");
    printf("    [1] Mostrar TODAS as mensagens\n");
    printf("    [2] Mostrar apenas as 10 ultimas\n");
    printf("    [3] Limpar buffer\n");
    printf("    [4] Ligar/desligar impressao em tempo real\n");
    int sub = ler_inteiro("    Escolha (1-4): ");

    if (sub == 1) {
        ocpp_console_imprimir(0);
    } else if (sub == 2) {
        ocpp_console_imprimir(10);
    } else if (sub == 3) {
        ocpp_console_limpar();
    } else if (sub == 4) {
        static int verboso_atual = 1;
        verboso_atual = !verboso_atual;
        ocpp_set_verboso(verboso_atual);
        printf("    [ocpp] Impressao em tempo real: %s\n\n",
               verboso_atual ? "LIGADA" : "DESLIGADA");
    } else {
        printf("    [aviso] Opcao invalida.\n\n");
    }
}


static void acao_relatorio(void) {
    printf("\n  >>> Relatorio operacional <<<\n");
    printf("    [1] Imprimir resumo na tela\n");
    printf("    [2] Gerar arquivo .txt em ./relatorios/\n");
    int sub = ler_inteiro("    Escolha (1-2): ");

    if (sub == 1) {
        relatorio_imprimir_tela();
    } else if (sub == 2) {
        relatorio_gerar_arquivo();
    } else {
        printf("    [aviso] Opcao invalida.\n\n");
    }
}


/* ===========================================================================
 *  Menu principal
 * ===========================================================================
 */

static void imprimir_menu(void) {
    float demanda_agora = demanda_total_atual();
    float ocupacao_pct  = (DEMANDA_CONTRATADA > 0.0f)
                          ? (demanda_agora / DEMANDA_CONTRATADA) * 100.0f
                          : 0.0f;

    printf("\n");
    printf("  =================================================================\n");
    printf("    ChargeGrid SmartHub - CLI v1.0\n");
    printf("    EV Challenge 2026 - FIAP x GoodWe\n");
    printf("  =================================================================\n");
    printf("    Relogio simulado:               %02d:%02d (%s)\n",
           hora_simulada, minuto_simulada, nome_faixa_horaria(hora_simulada));
    printf("    Demanda contratada:             %.1f kW\n",
           (float)DEMANDA_CONTRATADA);
    printf("    Demanda em uso:                 %.2f kW (%.1f%%)\n",
           demanda_agora, ocupacao_pct);
    printf("  -----------------------------------------------------------------\n");
    printf("    [1]  Iniciar nova sessao de recarga\n");
    printf("    [2]  Listar todas as sessoes\n");
    printf("    [3]  Detalhe de uma sessao\n");
    printf("    [4]  Encerrar sessao\n");
    printf("    [5]  Avancar tempo simulado\n");
    printf("    [6]  Aplicar controle de demanda (load balancing)\n");
    printf("    [7]  Tarifa dinamica - decomposicao e recalculo\n");
    printf("    [8]  Console OCPP - mensagens trafegadas\n");
    printf("    [9]  Gerar relatorio operacional\n");
    printf("    [0]  Sair\n");
    printf("  =================================================================\n");
}


int main(void) {
    sessao_inicializar_sistema();

    /*
     *  Em OCPP 1.6, a primeira mensagem que um carregador envia ao Central
     *  System ao ligar e a BootNotification, contendo vendor e model. O CSMS
     *  responde com status (Accepted/Pending/Rejected) e o intervalo de
     *  Heartbeat que deve ser usado. Emitimos esse par no boot da aplicacao.
     */
    ocpp_emit_boot_notification("GoodWe", "ChargeGrid-SmartHub-v1.0");

    int opcao;
    do {
        imprimir_menu();
        opcao = ler_inteiro("    Opcao: ");

        switch (opcao) {
            case 1: acao_nova_sessao();      break;
            case 2: sessao_listar();         break;
            case 3: acao_detalhe_sessao();   break;
            case 4: acao_encerrar_sessao();  break;
            case 5: acao_avancar_tempo();    break;
            case 6: acao_controle_demanda(); break;
            case 7: acao_tarifa();           break;
            case 8: acao_ocpp_console();     break;
            case 9: acao_relatorio();        break;
            case 0:
                printf("\n  Encerrando ChargeGrid SmartHub. Ate a proxima.\n\n");
                break;
            default:
                printf("\n  [aviso] Opcao invalida.\n\n");
        }
    } while (opcao != 0);

    return 0;
}
