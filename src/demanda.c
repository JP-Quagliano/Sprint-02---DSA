/*
 * ============================================================================
 *  demanda.c  -  Implementacao do controle de demanda (load balancing)
 * ============================================================================
 */

#include <stdio.h>
#include "demanda.h"
#include "sessao.h"
#include "ocpp.h"


/* ===========================================================================
 *  AUXILIARES INTERNAS
 *
 *  static = visivel so dentro deste arquivo. Outros .c nao enxergam.
 *  E como "private" em outras linguagens.
 * ===========================================================================
 */

/*
 *  e_sessao_elegivel  -  retorna 1 se a sessao deve participar do
 *  algoritmo de alocacao. Sessoes finalizadas, em erro ou aguardando
 *  autorizacao nao consomem potencia.
 */
static int e_sessao_elegivel(const Sessao *s) {
    return (s->estado == CARREGANDO) || (s->estado == PAUSADA_DEMANDA);
}

/*
 *  soma_pedidos_da_prioridade  -  soma a potencia NOMINAL de todas as
 *  sessoes elegiveis de uma determinada prioridade. Isso e o "pedido"
 *  daquela prioridade ao sistema.
 */
static float soma_pedidos_da_prioridade(int prioridade) {
    float total = 0.0f;
    for (int i = 0; i < total_sessoes; i++) {
        const Sessao *s = &sessoes[i];
        if (e_sessao_elegivel(s) && s->prioridade == prioridade) {
            total += s->potencia_nominal_kw;
        }
    }
    return total;
}

/*
 *  aplicar_alocacao_para_prioridade  -  faz a alocacao real para uma
 *  prioridade especifica, atualizando potencia_atual_kw de cada sessao
 *  daquela prioridade.
 *
 *  Parametros:
 *     prioridade           - qual tier estamos atendendo (1, 2 ou 3)
 *     orcamento_disponivel - quanto kW ainda podemos distribuir
 *
 *  Retorna: quanto kW foi efetivamente consumido nesta prioridade
 *           (vai sair do orcamento total do edificio).
 */
static float aplicar_alocacao_para_prioridade(int prioridade,
                                              float orcamento_disponivel) {
    float total_pedido = soma_pedidos_da_prioridade(prioridade);

    if (total_pedido <= 0.0f) {
        return 0.0f;  /* ninguem nesta prioridade */
    }

    if (total_pedido <= orcamento_disponivel) {
        /*
         *  Caso confortavel: o orcamento cobre todos os pedidos desta
         *  prioridade. Cada sessao recebe o nominal cheio.
         */
        for (int i = 0; i < total_sessoes; i++) {
            Sessao *s = &sessoes[i];
            if (e_sessao_elegivel(s) && s->prioridade == prioridade) {
                s->potencia_atual_kw = s->potencia_nominal_kw;
            }
        }
        return total_pedido;
    }

    /*
     *  Caso apertado: o pedido excede o orcamento. Dividimos proporcionalmente.
     *  Cada sessao recebe uma fatia equivalente a (pedido_dela / pedido_total).
     */
    float fator = orcamento_disponivel / total_pedido;
    for (int i = 0; i < total_sessoes; i++) {
        Sessao *s = &sessoes[i];
        if (e_sessao_elegivel(s) && s->prioridade == prioridade) {
            s->potencia_atual_kw = s->potencia_nominal_kw * fator;
        }
    }
    return orcamento_disponivel;
}

/*
 *  ajustar_estados_pos_alocacao  -  apos a redistribuicao de potencia,
 *  alguns carregadores podem ter ficado abaixo de MIN_POTENCIA_CARGA
 *  (1.4 kW segundo IEC 61851). Esses entram em PAUSADA_DEMANDA.
 *  Sessoes que voltam acima do limiar reativam o estado CARREGANDO.
 */
static int ajustar_estados_pos_alocacao(void) {
    int mudancas = 0;
    for (int i = 0; i < total_sessoes; i++) {
        Sessao *s = &sessoes[i];
        if (!e_sessao_elegivel(s)) continue;

        if (s->potencia_atual_kw < MIN_POTENCIA_CARGA) {
            if (s->estado != PAUSADA_DEMANDA) {
                s->estado = PAUSADA_DEMANDA;
                s->potencia_atual_kw = 0.0f;
                mudancas++;
                /*
                 *  Em OCPP 1.6, quando o carregador suspende a entrega
                 *  por decisao do central system, o status apropriado e
                 *  SuspendedEVSE (Electric Vehicle Supply Equipment).
                 */
                ocpp_emit_status_notification(s->id, "SuspendedEVSE", "NoError");
            }
        } else {
            if (s->estado != CARREGANDO) {
                s->estado = CARREGANDO;
                mudancas++;
                /* Retomou - volta para Charging */
                ocpp_emit_status_notification(s->id, "Charging", "NoError");
            }
        }
    }
    return mudancas;
}


/* ===========================================================================
 *  FUNCOES PUBLICAS
 * ===========================================================================
 */

int demanda_aplicar(void) {
    /*
     *  1) Calculamos o pedido bruto: quanto cada sessao gostaria de receber
     *     se nao houvesse restricao. Isso e a soma dos nominais.
     */
    float pedido_bruto = 0.0f;
    for (int i = 0; i < total_sessoes; i++) {
        if (e_sessao_elegivel(&sessoes[i])) {
            pedido_bruto += sessoes[i].potencia_nominal_kw;
        }
    }

    printf("\n");
    printf("  =================================================================\n");
    printf("    Controle de demanda - relatorio de alocacao\n");
    printf("  =================================================================\n");
    printf("    Demanda contratada do edificio.... %.1f kW\n",
           (float)DEMANDA_CONTRATADA);
    printf("    Pedido bruto (sem restricao)...... %.2f kW\n", pedido_bruto);

    /*
     *  2) Se o pedido cabe folgado no orcamento, ninguem precisa ser
     *     limitado. Cada sessao recebe seu nominal.
     */
    if (pedido_bruto <= DEMANDA_CONTRATADA) {
        printf("    Situacao.......................... FOLGA - sem throttling\n");
        for (int i = 0; i < total_sessoes; i++) {
            Sessao *s = &sessoes[i];
            if (e_sessao_elegivel(s)) {
                s->potencia_atual_kw = s->potencia_nominal_kw;
            }
        }
        ajustar_estados_pos_alocacao();
        printf("  =================================================================\n\n");
        return 0;
    }

    /*
     *  3) Estamos em sobrecarga. Distribuimos por prioridade decrescente.
     *     Cada chamada de aplicar_alocacao_para_prioridade gasta parte do
     *     orcamento e devolve quanto foi gasto.
     */
    printf("    Situacao.......................... SOBRECARGA - aplicando triagem\n");
    printf("  -----------------------------------------------------------------\n");

    float orcamento = DEMANDA_CONTRATADA;

    float gasto_p3 = aplicar_alocacao_para_prioridade(3, orcamento);
    orcamento -= gasto_p3;
    printf("    Prioridade 3 (emergencia)......... gastou %.2f kW (restam %.2f)\n",
           gasto_p3, orcamento);

    float gasto_p2 = aplicar_alocacao_para_prioridade(2, orcamento);
    orcamento -= gasto_p2;
    printf("    Prioridade 2 (frota).............. gastou %.2f kW (restam %.2f)\n",
           gasto_p2, orcamento);

    float gasto_p1 = aplicar_alocacao_para_prioridade(1, orcamento);
    orcamento -= gasto_p1;
    printf("    Prioridade 1 (normal)............. gastou %.2f kW (restam %.2f)\n",
           gasto_p1, orcamento);

    /*
     *  4) Sessoes alocadas abaixo do minimo IEC 61851 sao pausadas.
     */
    int trocas = ajustar_estados_pos_alocacao();
    printf("  -----------------------------------------------------------------\n");
    printf("    Mudancas de estado pos-alocacao... %d\n", trocas);
    printf("  =================================================================\n\n");

    return 1;
}


void demanda_imprimir_status(void) {
    float total_p1 = 0.0f, total_p2 = 0.0f, total_p3 = 0.0f;
    int cnt_p1 = 0, cnt_p2 = 0, cnt_p3 = 0;

    for (int i = 0; i < total_sessoes; i++) {
        const Sessao *s = &sessoes[i];
        if (!e_sessao_elegivel(s)) continue;

        if (s->estado != CARREGANDO) continue;  /* status mostra somente o que esta entregando */

        switch (s->prioridade) {
            case 1: total_p1 += s->potencia_atual_kw; cnt_p1++; break;
            case 2: total_p2 += s->potencia_atual_kw; cnt_p2++; break;
            case 3: total_p3 += s->potencia_atual_kw; cnt_p3++; break;
        }
    }

    float total = total_p1 + total_p2 + total_p3;
    float ocupacao = (DEMANDA_CONTRATADA > 0.0f)
                     ? (total / DEMANDA_CONTRATADA) * 100.0f
                     : 0.0f;

    printf("\n");
    printf("  =================================================================\n");
    printf("    Status de demanda por prioridade\n");
    printf("  =================================================================\n");
    printf("    Prioridade 3 (emergencia): %d sessao(oes) - %.2f kW\n",
           cnt_p3, total_p3);
    printf("    Prioridade 2 (frota):      %d sessao(oes) - %.2f kW\n",
           cnt_p2, total_p2);
    printf("    Prioridade 1 (normal):     %d sessao(oes) - %.2f kW\n",
           cnt_p1, total_p1);
    printf("  -----------------------------------------------------------------\n");
    printf("    Total em uso:              %.2f kW de %.1f contratados (%.1f%%)\n",
           total, (float)DEMANDA_CONTRATADA, ocupacao);
    printf("  =================================================================\n\n");
}
