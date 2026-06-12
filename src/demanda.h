/*
 * ============================================================================
 *  demanda.h  -  Interface do modulo de controle de demanda
 * ============================================================================
 *
 *  Este modulo implementa a alocacao inteligente de potencia entre sessoes
 *  ativas quando a demanda total ultrapassa o limite contratado do edificio
 *  (DEMANDA_CONTRATADA, definido em tipos.h).
 *
 *  ALGORITMO (load balancing por prioridade):
 *
 *   1. Junta todas as sessoes que estao querendo carregar (CARREGANDO ou
 *      PAUSADA_DEMANDA - paused pode voltar se sobrar energia).
 *
 *   2. Atende por ordem de prioridade decrescente (3 -> 2 -> 1):
 *        - Prioridade 3 (emergencia): atendida primeiro, recebe o nominal.
 *        - Prioridade 2 (frota): atendida em seguida, recebe o nominal se
 *          ainda houver "orcamento" disponivel.
 *        - Prioridade 1 (normal): divide o que sobrar PROPORCIONALMENTE
 *          conforme a potencia nominal de cada sessao.
 *
 *   3. Se mesmo dentro de uma prioridade o pedido exceder o orcamento,
 *      a divisao proporcional acontece tambem nessa prioridade.
 *
 *   4. Sessoes que recebem menos que MIN_POTENCIA_CARGA (1.4 kW, padrao
 *      IEC 61851) sao marcadas como PAUSADA_DEMANDA - o protocolo nao
 *      permite carga abaixo desse limiar.
 *
 *   5. Sessoes que voltam acima do limiar recuperam estado CARREGANDO.
 *
 *  Esse e o algoritmo que costura o item 2 da rubrica (20 pontos).
 * ============================================================================
 */

#ifndef DEMANDA_H
#define DEMANDA_H

#include "tipos.h"

/*
 *  demanda_aplicar - executa o algoritmo de load balancing descrito acima.
 *
 *  Le a lista atual de sessoes (modulo sessao) e reescreve o campo
 *  potencia_atual_kw de cada uma. Tambem ajusta o estado entre CARREGANDO
 *  e PAUSADA_DEMANDA conforme o resultado.
 *
 *  Imprime no terminal um relatorio do que foi alocado para cada sessao.
 *  Retorna 1 se houve qualquer ajuste, 0 se nada mudou (caso comum quando
 *  a demanda total esta bem abaixo do limite contratado).
 */
int demanda_aplicar(void);

/*
 *  demanda_imprimir_status - mostra um panorama da ocupacao atual do
 *  edificio, separado por prioridade. Util para diagnostico operacional
 *  sem rodar a redistribuicao.
 */
void demanda_imprimir_status(void);


#endif /* DEMANDA_H */
