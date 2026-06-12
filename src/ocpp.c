/*
 * ============================================================================
 *  ocpp.c  -  Implementacao da simulacao do protocolo OCPP 1.6
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "ocpp.h"
#include "sessao.h"   /* para usar hora_simulada/minuto_simulada nos timestamps */


/* ===========================================================================
 *  ESTADO DO MODULO
 * ===========================================================================
 */

MensagemOCPP mensagens_ocpp[MAX_MENSAGENS_OCPP];
int total_mensagens_ocpp = 0;

/* Contador para gerar messageIds unicos (sequencial). */
static int contador_msg_id = 1;

/* Modo verboso: imprime cada mensagem no momento em que e emitida. */
static int verboso = 1;


/* ===========================================================================
 *  AUXILIARES INTERNAS
 * ===========================================================================
 */

/*
 *  gerar_message_id - cria um messageId no estilo do OCPP.
 *  Em producao seriam UUIDs; aqui usamos hexadecimal sequencial,
 *  que e suficiente para correlacionar Call com CallResult.
 */
static void gerar_message_id(char *destino, int tamanho) {
    snprintf(destino, tamanho, "msg-%05x", contador_msg_id++);
}

/*
 *  gerar_timestamp - formata o relogio simulado no padrao ISO 8601 Z
 *  (UTC). Usamos uma data fixa (2026-06-12) e a hora simulada do sistema.
 */
static void gerar_timestamp(char *destino, int tamanho) {
    snprintf(destino, tamanho, "2026-06-12T%02d:%02d:00Z",
             hora_simulada, minuto_simulada);
}

/*
 *  registrar - adiciona uma mensagem ao buffer e imprime se modo verboso.
 *  Se o buffer estiver cheio, descarta silenciosamente (politica simples;
 *  em producao seria circular).
 */
static void registrar(int tipo,
                      char direcao,
                      const char *message_id,
                      const char *action,
                      const char *payload) {
    if (total_mensagens_ocpp >= MAX_MENSAGENS_OCPP) {
        return;  /* buffer cheio - mensagem descartada */
    }

    MensagemOCPP *m = &mensagens_ocpp[total_mensagens_ocpp];
    m->id_local = total_mensagens_ocpp + 1;
    strncpy(m->message_id, message_id, sizeof(m->message_id) - 1);
    m->message_id[sizeof(m->message_id) - 1] = '\0';
    m->tipo = tipo;
    m->direcao = direcao;
    gerar_timestamp(m->timestamp, sizeof(m->timestamp));
    strncpy(m->action, action, sizeof(m->action) - 1);
    m->action[sizeof(m->action) - 1] = '\0';
    strncpy(m->payload, payload, sizeof(m->payload) - 1);
    m->payload[sizeof(m->payload) - 1] = '\0';

    total_mensagens_ocpp++;

    if (verboso) {
        /*
         *  Formato de impressao em tempo real - imita um "tail -f" no
         *  log do CSMS. As setas '>' e '<' indicam direcao da mensagem.
         */
        if (tipo == OCPP_CALL) {
            printf("    [OCPP %s %c] %s | [%d, \"%s\", \"%s\", %s]\n",
                   m->timestamp, direcao, action, tipo, m->message_id,
                   action, payload);
        } else {
            printf("    [OCPP %s %c] %s reply | [%d, \"%s\", %s]\n",
                   m->timestamp, direcao, action, tipo, m->message_id,
                   payload);
        }
    }
}


/*
 *  emitir_par - emite um par Call + CallResult com payloads especificos.
 *
 *  Em OCPP real cada Call gera um CallResult correspondente vindo do
 *  outro lado. Aqui simulamos o ciclo completo: Call sobe (Charger ->
 *  Central System) e CallResult desce (Central System -> Charger).
 */
static void emitir_par(const char *action,
                       const char *payload_call,
                       const char *payload_result,
                       char direcao_call) {
    char msg_id[16];
    gerar_message_id(msg_id, sizeof(msg_id));

    /* Direcao da resposta e sempre inversa */
    char direcao_result = (direcao_call == '>') ? '<' : '>';

    registrar(OCPP_CALL,        direcao_call,   msg_id, action, payload_call);
    registrar(OCPP_CALL_RESULT, direcao_result, msg_id, action, payload_result);
}


/* ===========================================================================
 *  EMISSORES PUBLICOS
 * ===========================================================================
 *
 *  Cada funcao formata o payload conforme o schema OCPP 1.6 e emite o par.
 *  Direcao '>' = Charge Point envia para Central System.
 *  Direcao '<' = Central System envia para Charge Point.
 */

void ocpp_emit_boot_notification(const char *vendor, const char *model) {
    char payload_call[256];
    char payload_result[256];

    snprintf(payload_call, sizeof(payload_call),
        "{\"chargePointVendor\":\"%s\",\"chargePointModel\":\"%s\"}",
        vendor, model);

    /*
     *  Resposta: o CSMS aceita o boot, define heartbeat de 60s e envia
     *  o timestamp atual (que o carregador usa para sincronizar relogio).
     */
    char ts[24];
    gerar_timestamp(ts, sizeof(ts));
    snprintf(payload_result, sizeof(payload_result),
        "{\"status\":\"Accepted\",\"currentTime\":\"%s\",\"interval\":60}",
        ts);

    emitir_par("BootNotification", payload_call, payload_result, '>');
}


void ocpp_emit_heartbeat(void) {
    char ts[24];
    gerar_timestamp(ts, sizeof(ts));

    char payload_result[64];
    snprintf(payload_result, sizeof(payload_result),
        "{\"currentTime\":\"%s\"}", ts);

    emitir_par("Heartbeat", "{}", payload_result, '>');
}


void ocpp_emit_authorize(const char *id_tag) {
    char payload_call[64];
    snprintf(payload_call, sizeof(payload_call),
        "{\"idTag\":\"%s\"}", id_tag);

    /*
     *  Resposta: status do cartao/app autenticado.
     *  Em producao, isso seria a verificacao real contra a base de usuarios.
     *  Aqui sempre aceitamos.
     */
    const char *payload_result = "{\"idTagInfo\":{\"status\":\"Accepted\"}}";

    emitir_par("Authorize", payload_call, payload_result, '>');
}


void ocpp_emit_start_transaction(int transaction_id,
                                 const char *id_tag,
                                 float meter_start) {
    char payload_call[256];
    char ts[24];
    gerar_timestamp(ts, sizeof(ts));

    /* meterStart em OCPP e em Wh (watt-hora), nao em kWh */
    int meter_start_wh = (int)(meter_start * 1000.0f);

    snprintf(payload_call, sizeof(payload_call),
        "{\"connectorId\":%d,\"idTag\":\"%s\",\"meterStart\":%d,\"timestamp\":\"%s\"}",
        transaction_id, id_tag, meter_start_wh, ts);

    char payload_result[128];
    snprintf(payload_result, sizeof(payload_result),
        "{\"transactionId\":%d,\"idTagInfo\":{\"status\":\"Accepted\"}}",
        transaction_id);

    emitir_par("StartTransaction", payload_call, payload_result, '>');
}


void ocpp_emit_meter_values(int transaction_id, float kwh, float power_kw) {
    /*
     *  MeterValues em OCPP carrega uma lista de sampledValue com
     *  diferentes 'measurand': Energy.Active.Import.Register (kWh)
     *  e Power.Active.Import (kW) sao os mais comuns.
     */
    char payload_call[256];
    char ts[24];
    gerar_timestamp(ts, sizeof(ts));

    snprintf(payload_call, sizeof(payload_call),
        "{\"connectorId\":%d,\"transactionId\":%d,\"meterValue\":[{\"timestamp\":\"%s\","
        "\"sampledValue\":[{\"value\":\"%.2f\",\"measurand\":\"Energy.Active.Import.Register\",\"unit\":\"kWh\"},"
        "{\"value\":\"%.2f\",\"measurand\":\"Power.Active.Import\",\"unit\":\"kW\"}]}]}",
        transaction_id, transaction_id, ts, kwh, power_kw);

    /* MeterValues nao tem payload de resposta (objeto vazio) */
    emitir_par("MeterValues", payload_call, "{}", '>');
}


void ocpp_emit_stop_transaction(int transaction_id,
                                float meter_stop,
                                const char *reason) {
    char payload_call[256];
    char ts[24];
    gerar_timestamp(ts, sizeof(ts));

    int meter_stop_wh = (int)(meter_stop * 1000.0f);

    snprintf(payload_call, sizeof(payload_call),
        "{\"transactionId\":%d,\"meterStop\":%d,\"timestamp\":\"%s\",\"reason\":\"%s\"}",
        transaction_id, meter_stop_wh, ts, reason);

    emitir_par("StopTransaction", payload_call,
               "{\"idTagInfo\":{\"status\":\"Accepted\"}}", '>');
}


void ocpp_emit_status_notification(int connector_id,
                                   const char *status,
                                   const char *error_code) {
    /*
     *  Status validos no OCPP 1.6: Available, Preparing, Charging,
     *  SuspendedEVSE, SuspendedEV, Finishing, Reserved, Unavailable, Faulted.
     *  errorCode normalmente "NoError" quando nao ha problema.
     */
    char payload_call[256];
    char ts[24];
    gerar_timestamp(ts, sizeof(ts));

    snprintf(payload_call, sizeof(payload_call),
        "{\"connectorId\":%d,\"status\":\"%s\",\"errorCode\":\"%s\",\"timestamp\":\"%s\"}",
        connector_id, status, error_code, ts);

    emitir_par("StatusNotification", payload_call, "{}", '>');
}


/* ===========================================================================
 *  CONSOLE
 * ===========================================================================
 */

void ocpp_console_imprimir(int n) {
    if (total_mensagens_ocpp == 0) {
        printf("\n  Nenhuma mensagem OCPP registrada ainda.\n\n");
        return;
    }

    int inicio = 0;
    if (n > 0 && n < total_mensagens_ocpp) {
        inicio = total_mensagens_ocpp - n;
    }

    printf("\n");
    printf("  =================================================================\n");
    printf("    Console OCPP - mensagens trafegadas (%d total)\n",
           total_mensagens_ocpp);
    printf("  =================================================================\n");

    for (int i = inicio; i < total_mensagens_ocpp; i++) {
        const MensagemOCPP *m = &mensagens_ocpp[i];
        const char *tipo_str = (m->tipo == OCPP_CALL)        ? "Call      "
                             : (m->tipo == OCPP_CALL_RESULT) ? "CallResult"
                             :                                  "CallError ";

        printf("  [%03d] %s %c %s %-22s | %s\n",
               m->id_local, m->timestamp, m->direcao, tipo_str,
               m->action, m->payload);
    }
    printf("  =================================================================\n\n");
}


void ocpp_console_limpar(void) {
    total_mensagens_ocpp = 0;
    contador_msg_id = 1;
    printf("  [ocpp] Buffer de mensagens limpo.\n\n");
}


int ocpp_total(void) {
    return total_mensagens_ocpp;
}


int ocpp_contar_por_action(const char *action) {
    int cnt = 0;
    for (int i = 0; i < total_mensagens_ocpp; i++) {
        /* Conta apenas Calls (nao duplica com CallResult) */
        if (mensagens_ocpp[i].tipo == OCPP_CALL &&
            strcmp(mensagens_ocpp[i].action, action) == 0) {
            cnt++;
        }
    }
    return cnt;
}


void ocpp_set_verboso(int v) {
    verboso = v;
}
