/*
 * ============================================================================
 *  ocpp.h  -  Simulacao do protocolo OCPP 1.6 (Open Charge Point Protocol)
 * ============================================================================
 *
 *  O OCPP e o protocolo padrao da industria para comunicacao entre
 *  carregadores de veiculos eletricos (Charge Point) e sistemas centrais
 *  (Central System Management Software - CSMS). A versao 1.6, lancada
 *  pela Open Charge Alliance em 2015, usa JSON sobre WebSocket.
 *
 *  Esta implementacao SIMULA o trafego das mensagens - nao ha rede real,
 *  mas a estrutura das mensagens segue o padrao do protocolo:
 *
 *      [ tipo, messageId, action, payload ]      <- Call (requisicao)
 *      [ tipo, messageId, payload ]              <- CallResult (resposta)
 *      [ tipo, messageId, errorCode, errorDesc ] <- CallError (erro)
 *
 *  Onde 'tipo' e: 2 = Call, 3 = CallResult, 4 = CallError.
 *
 *  Actions implementadas (subset do OCPP 1.6):
 *      - BootNotification     - carregador anuncia presenca ao CSMS
 *      - Heartbeat            - keep-alive periodico
 *      - Authorize            - validacao de idTag (cartao/app) do usuario
 *      - StartTransaction     - inicio de sessao de recarga
 *      - MeterValues          - leitura periodica de medidor (kWh, kW, A)
 *      - StopTransaction      - fim de sessao de recarga
 *      - StatusNotification   - mudanca de estado do connector
 *
 *  Este modulo cobre o item 4 da rubrica (15 pontos).
 * ============================================================================
 */

#ifndef OCPP_H
#define OCPP_H

#include "tipos.h"

/* Tipos de mensagem do protocolo (constantes do OCPP-J 1.6) */
#define OCPP_CALL          2
#define OCPP_CALL_RESULT   3
#define OCPP_CALL_ERROR    4

/* Tamanho maximo do buffer de mensagens (suficiente para demos extensos) */
#define MAX_MENSAGENS_OCPP 200


/*
 *  MensagemOCPP  -  representa uma mensagem trafegada no canal simulado.
 *
 *  Em OCPP real, payload e um objeto JSON. Aqui, guardamos como string
 *  formatada que IMITA JSON para fins didaticos e de exibicao no console.
 */
typedef struct {
    int  id_local;              /* contador sequencial (1, 2, 3...)         */
    char message_id[16];        /* messageId no padrao OCPP (uuid-like)     */
    int  tipo;                  /* OCPP_CALL, OCPP_CALL_RESULT, OCPP_CALL_ERROR */
    char direcao;               /* '>' = CP->CSMS, '<' = CSMS->CP            */
    char timestamp[24];         /* ISO8601-like: 2026-06-12T18:30:00Z       */
    char action[32];            /* "BootNotification", "MeterValues", etc.  */
    char payload[256];          /* corpo da mensagem (JSON-like)            */
} MensagemOCPP;


/* Buffer compartilhado, acessivel pelo modulo de relatorio para estatisticas */
extern MensagemOCPP mensagens_ocpp[MAX_MENSAGENS_OCPP];
extern int total_mensagens_ocpp;


/* ===========================================================================
 *  EMISSORES - chamados pelos outros modulos quando eventos acontecem
 * ===========================================================================
 *
 *  Cada funcao gera o par Call + CallResult e adiciona ao buffer.
 *  Tambem imprime no console em tempo real (modo "tail de log").
 */

void ocpp_emit_boot_notification(const char *vendor, const char *model);
void ocpp_emit_heartbeat(void);
void ocpp_emit_authorize(const char *id_tag);
void ocpp_emit_start_transaction(int transaction_id, const char *id_tag, float meter_start);
void ocpp_emit_meter_values(int transaction_id, float kwh, float power_kw);
void ocpp_emit_stop_transaction(int transaction_id, float meter_stop, const char *reason);
void ocpp_emit_status_notification(int connector_id, const char *status, const char *error_code);


/* ===========================================================================
 *  CONSOLE - visualizacao das mensagens trafegadas
 * ===========================================================================
 */

/*
 *  ocpp_console_imprimir - mostra as ultimas N mensagens do buffer.
 *  Se n <= 0, mostra todas.
 */
void ocpp_console_imprimir(int n);

/*
 *  ocpp_console_limpar - zera o buffer de mensagens.
 *  Util quando o operador quer "comecar do zero" um cenario de demo.
 */
void ocpp_console_limpar(void);


/* ===========================================================================
 *  ESTATISTICAS - usadas pelo modulo de relatorio
 * ===========================================================================
 */

int ocpp_total(void);
int ocpp_contar_por_action(const char *action);


/* ===========================================================================
 *  CONTROLE DE VERBOSIDADE
 * ===========================================================================
 *
 *  Por padrao, cada mensagem emitida tambem e impressa no console em tempo
 *  real. Isso e bom para demonstracao mas pode poluir a tela em testes
 *  longos. Use ocpp_set_verboso(0) para silenciar (mensagens continuam
 *  sendo gravadas no buffer e visiveis via opcao 8 do menu).
 */
void ocpp_set_verboso(int verboso);


#endif /* OCPP_H */
