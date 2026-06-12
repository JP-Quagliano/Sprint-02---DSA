/*
 * ============================================================================
 *  relatorio.c  -  Implementacao do relatorio operacional
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "relatorio.h"
#include "sessao.h"
#include "tarifa.h"
#include "ocpp.h"


/* ===========================================================================
 *  COLETORES DE METRICAS
 *
 *  Sao funcoes auxiliares que varrem o array de sessoes acumulando estatisticas
 *  em estruturas locais. Mantemos a logica de coleta separada da impressao.
 * ===========================================================================
 */

typedef struct {
    int   sessoes_total;
    int   sessoes_carregando;
    int   sessoes_pausadas;
    int   sessoes_finalizadas;
    int   sessoes_erro;

    int   por_modelo[3];      /* indice = ModeloHCA */
    int   por_prioridade[4];  /* indice 1..3, indice 0 nao usado */

    float kwh_total;
    float kwh_max_sessao;

    float receita_total;
    float receita_por_modelo[3];

    float demanda_atual;
    float ocupacao_pct;
} ResumoMetricas;


static void coletar_metricas(ResumoMetricas *r) {
    /* Zera tudo - importante porque a struct foi criada na stack */
    memset(r, 0, sizeof(*r));

    for (int i = 0; i < total_sessoes; i++) {
        const Sessao *s = &sessoes[i];

        r->sessoes_total++;

        switch (s->estado) {
            case CARREGANDO:      r->sessoes_carregando++;  break;
            case PAUSADA_DEMANDA: r->sessoes_pausadas++;    break;
            case FINALIZADA:      r->sessoes_finalizadas++; break;
            case ERRO:            r->sessoes_erro++;        break;
            default: break;
        }

        if (s->modelo >= 0 && s->modelo <= 2) {
            r->por_modelo[s->modelo]++;
            r->receita_por_modelo[s->modelo] += s->custo_acumulado;
        }
        if (s->prioridade >= 1 && s->prioridade <= 3) {
            r->por_prioridade[s->prioridade]++;
        }

        r->kwh_total     += s->kwh_entregue;
        r->receita_total += s->custo_acumulado;
        if (s->kwh_entregue > r->kwh_max_sessao) {
            r->kwh_max_sessao = s->kwh_entregue;
        }
    }

    r->demanda_atual = demanda_total_atual();
    r->ocupacao_pct  = (DEMANDA_CONTRATADA > 0.0f)
                       ? (r->demanda_atual / DEMANDA_CONTRATADA) * 100.0f
                       : 0.0f;
}


/* ===========================================================================
 *  IMPRESSAO - funcao generica que recebe um FILE* e escreve no destino
 *
 *  Truque fundamental: stdout TAMBEM e um FILE*. Entao a mesma funcao serve
 *  tanto para imprimir no terminal quanto para escrever em arquivo.
 *  Isso evita duplicar codigo de formatacao.
 * ===========================================================================
 */

static void escrever_relatorio(FILE *destino) {
    ResumoMetricas r;
    coletar_metricas(&r);

    float ticket_medio = (r.sessoes_total > 0)
                         ? (r.receita_total / r.sessoes_total)
                         : 0.0f;
    float kwh_medio = (r.sessoes_total > 0)
                      ? (r.kwh_total / r.sessoes_total)
                      : 0.0f;

    fprintf(destino, "\n");
    fprintf(destino, "================================================================\n");
    fprintf(destino, "  ChargeGrid SmartHub - Relatorio Operacional\n");
    fprintf(destino, "  EV Challenge 2026 - FIAP x GoodWe\n");
    fprintf(destino, "  Gerado as %02d:%02d (relogio simulado)\n",
            hora_simulada, minuto_simulada);
    fprintf(destino, "================================================================\n");

    /* ----- 1. PANORAMA OPERACIONAL ---------------------------------------- */
    fprintf(destino, "\n  [1] Panorama operacional\n");
    fprintf(destino, "  ----------------------------------------------------------------\n");
    fprintf(destino, "    Sessoes registradas:           %d\n",  r.sessoes_total);
    fprintf(destino, "      em CARREGANDO:               %d\n",  r.sessoes_carregando);
    fprintf(destino, "      em PAUSADA_DEMANDA:          %d\n",  r.sessoes_pausadas);
    fprintf(destino, "      FINALIZADAS:                 %d\n",  r.sessoes_finalizadas);
    fprintf(destino, "      em ERRO:                     %d\n",  r.sessoes_erro);
    fprintf(destino, "\n");
    fprintf(destino, "    Distribuicao por modelo HCA G2:\n");
    fprintf(destino, "      GW7K-HCA-20 (7 kW):          %d\n",  r.por_modelo[0]);
    fprintf(destino, "      GW11K-HCA-20 (11 kW):        %d\n",  r.por_modelo[1]);
    fprintf(destino, "      GW22K-HCA-20 (22 kW):        %d\n",  r.por_modelo[2]);
    fprintf(destino, "\n");
    fprintf(destino, "    Distribuicao por prioridade:\n");
    fprintf(destino, "      Normal (1):                  %d\n",  r.por_prioridade[1]);
    fprintf(destino, "      Frota (2):                   %d\n",  r.por_prioridade[2]);
    fprintf(destino, "      Emergencia (3):              %d\n",  r.por_prioridade[3]);

    /* ----- 2. ENERGIA ----------------------------------------------------- */
    fprintf(destino, "\n  [2] Energia entregue\n");
    fprintf(destino, "  ----------------------------------------------------------------\n");
    fprintf(destino, "    kWh total entregue:            %.2f kWh\n", r.kwh_total);
    fprintf(destino, "    kWh medio por sessao:          %.2f kWh\n", kwh_medio);
    fprintf(destino, "    kWh maximo em uma sessao:      %.2f kWh\n", r.kwh_max_sessao);
    fprintf(destino, "    Demanda contratada:            %.1f kW\n",  (float)DEMANDA_CONTRATADA);
    fprintf(destino, "    Demanda atual em uso:          %.2f kW (%.1f%%)\n",
            r.demanda_atual, r.ocupacao_pct);

    /* ----- 3. FINANCEIRO -------------------------------------------------- */
    fprintf(destino, "\n  [3] Performance financeira\n");
    fprintf(destino, "  ----------------------------------------------------------------\n");
    fprintf(destino, "    Receita total:                 R$ %.2f\n",  r.receita_total);
    fprintf(destino, "    Ticket medio por sessao:       R$ %.2f\n",  ticket_medio);
    fprintf(destino, "\n");
    fprintf(destino, "    Receita por modelo:\n");
    fprintf(destino, "      GW7K-HCA-20:                 R$ %.2f\n",  r.receita_por_modelo[0]);
    fprintf(destino, "      GW11K-HCA-20:                R$ %.2f\n",  r.receita_por_modelo[1]);
    fprintf(destino, "      GW22K-HCA-20:                R$ %.2f\n",  r.receita_por_modelo[2]);

    /* ----- 4. CONTEXTO TARIFARIO ----------------------------------------- */
    fprintf(destino, "\n  [4] Contexto tarifario atual\n");
    fprintf(destino, "  ----------------------------------------------------------------\n");
    fprintf(destino, "    Hora simulada:                 %02d:%02d\n",
            hora_simulada, minuto_simulada);
    fprintf(destino, "    Faixa horaria:                 %s\n",
            nome_faixa_horaria(hora_simulada));
    fprintf(destino, "    Faixa de ocupacao:             %s\n",
            nome_faixa_ocupacao(r.demanda_atual, DEMANDA_CONTRATADA));
    fprintf(destino, "    Tarifa GW7K  agora:            R$ %.3f / kWh\n",
            tarifa_calcular(hora_simulada, r.demanda_atual, DEMANDA_CONTRATADA, GW7K));
    fprintf(destino, "    Tarifa GW11K agora:            R$ %.3f / kWh\n",
            tarifa_calcular(hora_simulada, r.demanda_atual, DEMANDA_CONTRATADA, GW11K));
    fprintf(destino, "    Tarifa GW22K agora:            R$ %.3f / kWh\n",
            tarifa_calcular(hora_simulada, r.demanda_atual, DEMANDA_CONTRATADA, GW22K));

    /* ----- 5. SAUDE DO CANAL OCPP ---------------------------------------- */
    fprintf(destino, "\n  [5] Trafego OCPP 1.6\n");
    fprintf(destino, "  ----------------------------------------------------------------\n");
    fprintf(destino, "    Mensagens total no buffer:     %d\n", ocpp_total());
    fprintf(destino, "    BootNotification (Calls):      %d\n",
            ocpp_contar_por_action("BootNotification"));
    fprintf(destino, "    Heartbeat (Calls):             %d\n",
            ocpp_contar_por_action("Heartbeat"));
    fprintf(destino, "    Authorize (Calls):             %d\n",
            ocpp_contar_por_action("Authorize"));
    fprintf(destino, "    StartTransaction (Calls):      %d\n",
            ocpp_contar_por_action("StartTransaction"));
    fprintf(destino, "    MeterValues (Calls):           %d\n",
            ocpp_contar_por_action("MeterValues"));
    fprintf(destino, "    StopTransaction (Calls):       %d\n",
            ocpp_contar_por_action("StopTransaction"));
    fprintf(destino, "    StatusNotification (Calls):    %d\n",
            ocpp_contar_por_action("StatusNotification"));

    /* ----- 6. DETALHE POR SESSAO ----------------------------------------- */
    fprintf(destino, "\n  [6] Detalhamento por sessao\n");
    fprintf(destino, "  ----------------------------------------------------------------\n");
    fprintf(destino, "  +------+----------+--------------+------------+--------+----------+---------+\n");
    fprintf(destino, "  |  ID  | Placa    | Modelo HCA   | Estado     | Prio.  |  kWh     |   R$    |\n");
    fprintf(destino, "  +------+----------+--------------+------------+--------+----------+---------+\n");
    for (int i = 0; i < total_sessoes; i++) {
        const Sessao *s = &sessoes[i];
        fprintf(destino, "  | %4d | %-8s | %-12s | %-10s | %6d | %8.2f | %7.2f |\n",
                s->id, s->placa_veiculo, nome_modelo(s->modelo),
                nome_estado(s->estado), s->prioridade,
                s->kwh_entregue, s->custo_acumulado);
    }
    fprintf(destino, "  +------+----------+--------------+------------+--------+----------+---------+\n");

    fprintf(destino, "\n================================================================\n");
    fprintf(destino, "  Fim do relatorio.\n");
    fprintf(destino, "================================================================\n\n");
}


/* ===========================================================================
 *  FUNCOES PUBLICAS
 * ===========================================================================
 */

void relatorio_imprimir_tela(void) {
    escrever_relatorio(stdout);
}


int relatorio_gerar_arquivo(void) {
    /*
     *  Nome do arquivo: relatorio_HHMM.txt baseado no relogio simulado.
     *  Sufixo seria o ideal mas a hora ja diferencia suficientemente entre
     *  execucoes de demo.
     */
    char caminho[64];
    snprintf(caminho, sizeof(caminho), "relatorios/relatorio_%02d%02d.txt",
             hora_simulada, minuto_simulada);

    FILE *f = fopen(caminho, "w");
    if (f == NULL) {
        printf("\n  [erro] Nao foi possivel criar %s. Verifique se a pasta\n"
               "         relatorios/ existe na raiz do projeto.\n\n", caminho);
        return 0;
    }

    escrever_relatorio(f);
    fclose(f);

    printf("\n  [ok] Relatorio gerado: %s\n", caminho);
    printf("       Abra com qualquer editor de texto para conferir.\n\n");
    return 1;
}
