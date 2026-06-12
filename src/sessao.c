/*
 * ============================================================================
 *  sessao.c  -  Implementacao do modulo de gerenciamento de sessoes
 * ============================================================================
 *
 *  Aqui vive a logica de verdade. Cada funcao declarada em sessao.h tem o
 *  seu corpo escrito aqui.
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "sessao.h"
#include "tarifa.h"
#include "ocpp.h"

/* ===========================================================================
 *  RESERVA DE MEMORIA DAS VARIAVEIS GLOBAIS
 * ===========================================================================
 *
 *  No header dissemos "extern" para anunciar que essas variaveis existem.
 *  Aqui (em apenas UM arquivo do projeto) elas sao de fato declaradas
 *  reservando memoria. Esse e o padrao "declarar no .h, definir no .c".
 */

Sessao sessoes[MAX_SESSOES];
int    total_sessoes = 0;
int    proximo_id    = 1;

/*
 *  Relogio simulado - inicializa em 09:00 (horario padrao de abertura
 *  de um shopping/varejo). E ajustavel em sessao_inicializar_sistema().
 */
int hora_simulada    = 9;
int minuto_simulada  = 0;


/* ===========================================================================
 *  FUNCOES AUXILIARES (privadas do modulo)
 * ===========================================================================
 */

/*
 * potencia_do_modelo  -  retorna a potencia nominal de cada modelo HCA G2.
 * Numeros baseados na linha real GoodWe ChargeGrid Intelligence.
 */
float potencia_do_modelo(ModeloHCA modelo) {
    switch (modelo) {
        case GW7K:  return 7.0f;
        case GW11K: return 11.0f;
        case GW22K: return 22.0f;
    }
    return 0.0f; /* nunca deveria chegar aqui */
}

const char *nome_estado(EstadoSessao e) {
    switch (e) {
        case OCIOSO:                 return "OCIOSO";
        case AGUARDANDO_AUTORIZACAO: return "AGUARDANDO";
        case CARREGANDO:             return "CARREGANDO";
        case PAUSADA_DEMANDA:        return "PAUSADA";
        case FINALIZADA:             return "FINALIZADA";
        case ERRO:                   return "ERRO";
    }
    return "?";
}

const char *nome_modelo(ModeloHCA m) {
    switch (m) {
        case GW7K:  return "GW7K-HCA-20";
        case GW11K: return "GW11K-HCA-20";
        case GW22K: return "GW22K-HCA-20";
    }
    return "?";
}

/*
 * busca_indice_por_id  -  varre o array procurando pelo ID informado.
 * Retorna o indice (0..total_sessoes-1) ou -1 se nao achou.
 *
 * "static" significa que essa funcao so e visivel dentro deste arquivo.
 * Outros .c nao conseguem ve-la. E uma funcao auxiliar interna.
 */
static int busca_indice_por_id(int id) {
    for (int i = 0; i < total_sessoes; i++) {
        if (sessoes[i].id == id) {
            return i;
        }
    }
    return -1;
}


/* ===========================================================================
 *  FUNCOES PUBLICAS - declaradas em sessao.h
 * ===========================================================================
 */

void sessao_inicializar_sistema(void) {
    total_sessoes = 0;
    proximo_id    = 1;
    /* Nao precisamos zerar o array - o C inicializa globais com 0 por padrao */
    printf("[sistema] Modulo de sessoes inicializado. Capacidade: %d sessoes.\n",
           MAX_SESSOES);
}


int sessao_criar(const char *placa,
                 ModeloHCA modelo,
                 int hora,
                 int minuto,
                 int prioridade) {
    /* Verificacao 1: capacidade do array */
    if (total_sessoes >= MAX_SESSOES) {
        printf("[erro] Capacidade maxima atingida (%d sessoes simultaneas).\n",
               MAX_SESSOES);
        return -1;
    }

    /* Verificacao 2: parametros validos */
    if (placa == NULL || strlen(placa) == 0) {
        printf("[erro] Placa do veiculo nao pode ser vazia.\n");
        return -1;
    }
    if (hora < 0 || hora > 23 || minuto < 0 || minuto > 59) {
        printf("[erro] Horario invalido (%02d:%02d).\n", hora, minuto);
        return -1;
    }
    if (prioridade < 1 || prioridade > 3) {
        printf("[erro] Prioridade deve ser 1, 2 ou 3.\n");
        return -1;
    }

    /* Cria a sessao na proxima posicao livre do array */
    int i = total_sessoes;
    Sessao *s = &sessoes[i];

    s->id = proximo_id++;

    /* strncpy + terminador garantem que nunca passamos do tamanho do buffer */
    strncpy(s->placa_veiculo, placa, TAMANHO_PLACA - 1);
    s->placa_veiculo[TAMANHO_PLACA - 1] = '\0';

    /* Identificador do carregador segue o padrao CHG-XXX-NN */
    snprintf(s->id_carregador, TAMANHO_ID_CARREG, "CHG-%03d", s->id);

    s->modelo               = modelo;
    s->estado               = AGUARDANDO_AUTORIZACAO;
    s->hora_inicio          = hora;
    s->minuto_inicio        = minuto;
    s->potencia_nominal_kw  = potencia_do_modelo(modelo);
    s->potencia_atual_kw    = 0.0f;
    s->kwh_entregue         = 0.0f;
    s->custo_acumulado      = 0.0f;
    s->prioridade           = prioridade;

    total_sessoes++;

    /* Apos autorizacao instantanea (simulada), entra em CARREGANDO */
    s->estado            = CARREGANDO;
    s->potencia_atual_kw = s->potencia_nominal_kw;

    printf("[ok] Sessao %d criada para %s no carregador %s (%s, %.1f kW).\n",
           s->id, s->placa_veiculo, s->id_carregador,
           nome_modelo(modelo), s->potencia_nominal_kw);

    /*
     *  Emite a sequencia OCPP padrao de inicio de sessao:
     *   1. Authorize         - validacao do idTag (placa = idTag aqui)
     *   2. StartTransaction  - inicia a transacao no central system
     *   3. StatusNotification - informa que o connector saiu de Available
     */
    ocpp_emit_authorize(s->placa_veiculo);
    ocpp_emit_start_transaction(s->id, s->placa_veiculo, 0.0f);
    ocpp_emit_status_notification(s->id, "Charging", "NoError");

    return s->id;
}


int sessao_encerrar(int id) {
    int i = busca_indice_por_id(id);
    if (i < 0) {
        printf("[erro] Sessao %d nao encontrada.\n", id);
        return 0;
    }

    Sessao *s = &sessoes[i];
    if (s->estado == FINALIZADA) {
        printf("[info] Sessao %d ja estava finalizada.\n", id);
        return 1;
    }

    s->estado            = FINALIZADA;
    s->potencia_atual_kw = 0.0f;

    printf("[ok] Sessao %d encerrada. Entregue: %.2f kWh. Cobrado: R$ %.2f.\n",
           id, s->kwh_entregue, s->custo_acumulado);

    /*
     *  Emite a sequencia OCPP padrao de fim de sessao:
     *   1. StopTransaction    - encerra a transacao com meterStop
     *   2. StatusNotification - connector volta para Available
     */
    ocpp_emit_stop_transaction(s->id, s->kwh_entregue, "EVDisconnected");
    ocpp_emit_status_notification(s->id, "Available", "NoError");

    return 1;
}


void sessao_listar(void) {
    if (total_sessoes == 0) {
        printf("\n  Nenhuma sessao registrada ate o momento.\n\n");
        return;
    }

    printf("\n");
    printf("  +------+----------+---------------+--------------+-----------+----------+---------+\n");
    printf("  |  ID  |  Placa   |  Carregador   |    Modelo    |  Estado   |  kWh     |  R$     |\n");
    printf("  +------+----------+---------------+--------------+-----------+----------+---------+\n");

    for (int i = 0; i < total_sessoes; i++) {
        Sessao *s = &sessoes[i];
        printf("  | %4d | %-8s | %-13s | %-12s | %-9s | %8.2f | %7.2f |\n",
               s->id,
               s->placa_veiculo,
               s->id_carregador,
               nome_modelo(s->modelo),
               nome_estado(s->estado),
               s->kwh_entregue,
               s->custo_acumulado);
    }
    printf("  +------+----------+---------------+--------------+-----------+----------+---------+\n\n");
}


int sessao_imprimir_detalhe(int id) {
    int i = busca_indice_por_id(id);
    if (i < 0) {
        printf("[erro] Sessao %d nao encontrada.\n", id);
        return 0;
    }
    Sessao *s = &sessoes[i];

    printf("\n  ----- Sessao %d -----\n", s->id);
    printf("    Veiculo (placa):      %s\n",       s->placa_veiculo);
    printf("    Carregador:           %s\n",       s->id_carregador);
    printf("    Modelo HCA G2:        %s\n",       nome_modelo(s->modelo));
    printf("    Estado:               %s\n",       nome_estado(s->estado));
    printf("    Inicio (simulado):    %02d:%02d\n",s->hora_inicio, s->minuto_inicio);
    printf("    Potencia nominal:     %.2f kW\n",  s->potencia_nominal_kw);
    printf("    Potencia atual:       %.2f kW\n",  s->potencia_atual_kw);
    printf("    Energia entregue:     %.2f kWh\n", s->kwh_entregue);
    printf("    Custo acumulado:      R$ %.2f\n",  s->custo_acumulado);
    printf("    Prioridade:           %d\n",       s->prioridade);
    printf("\n");
    return 1;
}


void sessao_avancar_tempo(int minutos) {
    if (minutos <= 0) return;

    /*
     *  Avancamos o relogio global primeiro. Se o operador adicionar 90 minutos
     *  e o relogio estiver em 18:30, queremos que ele va para 20:00 (e nao
     *  que ele "estoure" para 20:60). O while resolve isso.
     */
    minuto_simulada += minutos;
    while (minuto_simulada >= 60) {
        minuto_simulada -= 60;
        hora_simulada = (hora_simulada + 1) % 24;
    }

    float horas = (float)minutos / 60.0f;
    float demanda_agora = demanda_total_atual();

    for (int i = 0; i < total_sessoes; i++) {
        Sessao *s = &sessoes[i];

        if (s->estado == CARREGANDO) {
            /* energia entregue no intervalo = potencia atual x tempo */
            float kwh_adicionado = s->potencia_atual_kw * horas;
            s->kwh_entregue     += kwh_adicionado;

            /*
             *  Cobranca usa a tarifa DINAMICA calculada agora.
             *  A funcao tarifa_calcular vive no modulo tarifa.c e considera
             *  3 variaveis: hora do dia, ocupacao do edificio e modelo HCA.
             */
            float tarifa_aplicada = tarifa_calcular(
                hora_simulada,
                demanda_agora,
                DEMANDA_CONTRATADA,
                s->modelo
            );
            s->custo_acumulado += kwh_adicionado * tarifa_aplicada;

            /*
             *  Emite MeterValues a cada ciclo de avanco de tempo - este e o
             *  comportamento real do OCPP, onde o carregador envia leituras
             *  periodicas ao CSMS (tipicamente a cada 60s no padrao).
             */
            ocpp_emit_meter_values(s->id, s->kwh_entregue, s->potencia_atual_kw);
        }
    }

    /*
     *  Heartbeat OCPP - em producao seria automatico a cada 60s.
     *  Aqui emitimos uma vez por ciclo de avancar tempo, sinalizando que
     *  o carregador continua online.
     */
    ocpp_emit_heartbeat();

    printf("[sistema] Tempo avancou %d minuto(s). Relogio simulado: %02d:%02d.\n",
           minutos, hora_simulada, minuto_simulada);
}


float demanda_total_atual(void) {
    float soma = 0.0f;
    for (int i = 0; i < total_sessoes; i++) {
        if (sessoes[i].estado == CARREGANDO) {
            soma += sessoes[i].potencia_atual_kw;
        }
    }
    return soma;
}
