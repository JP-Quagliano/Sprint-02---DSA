/*
 * ============================================================================
 *  tarifa.h  -  Interface do modulo de tarifacao dinamica
 * ============================================================================
 *
 *  O preco por kWh nao e fixo - ele varia conforme TRES variaveis:
 *
 *  1) HORARIO            - tarifa de ponta (18-21h) e mais cara, fora-ponta
 *                          (22-06h) e mais barata, intermediario fica neutro.
 *                          Modelo inspirado na estrutura tarifaria branca
 *                          da ANEEL (Resolucao Normativa 733/2016).
 *
 *  2) DEMANDA DO PREDIO  - quando o edificio esta perto do limite contratado,
 *                          o sistema aplica sobretaxa de pico para desencorajar
 *                          recargas adicionais (mesma logica de "surge pricing"
 *                          de aplicativos de mobilidade).
 *
 *  3) MODELO DO CARREGADOR - carregadores mais rapidos (GW22K) cobram premium
 *                          por entregar energia em menos tempo. Carregadores
 *                          de longa permanencia (GW7K) usam a tarifa base.
 *
 *  A tarifa final e o produto dos tres multiplicadores aplicados sobre
 *  TARIFA_BASE (definida em tipos.h).
 * ============================================================================
 */

#ifndef TARIFA_H
#define TARIFA_H

#include "tipos.h"

/*
 *  tarifa_calcular - retorna o preco por kWh para o contexto atual.
 *
 *  Parametros:
 *     hora                - hora simulada (0 a 23)
 *     demanda_atual       - soma de potencia em uso agora (kW)
 *     demanda_contratada  - limite do edificio (kW) - normalmente 75
 *     modelo              - modelo HCA G2 da sessao
 *
 *  Retorna: preco em R$/kWh, ja aplicados os tres multiplicadores.
 */
float tarifa_calcular(int hora,
                      float demanda_atual,
                      float demanda_contratada,
                      ModeloHCA modelo);

/*
 *  tarifa_recalcular_sessoes - aplica a tarifa atual a todas as sessoes
 *  CARREGANDO, atualizando o custo acumulado com base nos kWh ja entregues.
 *
 *  Funcao usada quando o operador quer "ajustar" os custos apos uma mudanca
 *  brusca de cenario (ex: passou da janela de ponta para fora-ponta).
 */
void tarifa_recalcular_sessoes(void);

/*
 *  tarifa_explicar - imprime no terminal a decomposicao dos multiplicadores
 *  para o contexto atual (hora simulada + demanda atual), exibindo o preco
 *  resultante para cada modelo HCA G2. Util para defesa oral e demonstracao.
 */
void tarifa_explicar(void);

/*
 *  Funcoes auxiliares expostas para uso no relatorio final (Parte 3).
 */
const char *nome_faixa_horaria(int hora);
const char *nome_faixa_ocupacao(float demanda_atual, float demanda_contratada);


#endif /* TARIFA_H */
