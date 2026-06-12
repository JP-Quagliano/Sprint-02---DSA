/*
 * ============================================================================
 *  relatorio.h  -  Geracao de relatorio operacional consolidado
 * ============================================================================
 *
 *  Modulo responsavel por consolidar todos os dados da simulacao em um
 *  relatorio auditavel - arquivo .txt salvo em ./relatorios/ - e tambem
 *  oferecer uma versao "tela" do resumo para inspecao rapida.
 *
 *  KPIs cobertos (alinhados com os indicadores do painel gestor do SmartHub
 *  citados nas outras disciplinas do EV Challenge):
 *
 *      OPERACIONAIS    sessoes totais, ativas, pausadas, finalizadas;
 *                      distribuicao por modelo HCA e por prioridade
 *
 *      ENERGIA         kWh total entregue, kWh medio por sessao,
 *                      pico de demanda observado, demanda media
 *
 *      FINANCEIROS     receita total, ticket medio por sessao,
 *                      receita por modelo HCA
 *
 *      TARIFARIOS      faixa horaria atual, multiplicadores em vigor
 *
 *      OCPP            total de mensagens, mensagens por action,
 *                      relacao Call / CallResult (saude do canal)
 * ============================================================================
 */

#ifndef RELATORIO_H
#define RELATORIO_H

/*
 *  relatorio_imprimir_tela - mostra o resumo no terminal.
 *  Util para inspecao rapida durante a operacao.
 */
void relatorio_imprimir_tela(void);

/*
 *  relatorio_gerar_arquivo - cria um arquivo .txt timbrado em ./relatorios/
 *  com o nome relatorio_HHMMSS.txt (HHMM e a hora simulada).
 *
 *  Retorna 1 em sucesso, 0 em falha (ex: nao conseguiu criar o arquivo).
 */
int relatorio_gerar_arquivo(void);


#endif /* RELATORIO_H */
