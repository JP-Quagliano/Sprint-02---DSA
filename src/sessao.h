/*
 * ============================================================================
 *  sessao.h  -  Interface do modulo de gerenciamento de sessoes
 * ============================================================================
 *
 *  Este header declara TUDO QUE O MODULO sessao OFERECE para os outros modulos.
 *  E o "cardapio publico" do modulo. A implementacao real esta em sessao.c.
 *
 *  Quem precisa criar/listar/encerrar sessoes deve dar #include "sessao.h"
 *  e depois usar as funcoes daqui. Nao precisa saber como elas funcionam por
 *  dentro - so precisa saber o que cada uma faz.
 * ============================================================================
 */

#ifndef SESSAO_H
#define SESSAO_H

#include "tipos.h"

/* ===========================================================================
 *  VARIAVEIS GLOBAIS COMPARTILHADAS
 * ===========================================================================
 *
 *  Decisao didatica importante: usamos um array global de tamanho fixo em vez
 *  de alocacao dinamica (malloc/free) porque o grupo ainda nao viu ponteiros
 *  avancados. O array tem MAX_SESSOES posicoes (definido em tipos.h).
 *
 *  A palavra "extern" diz ao compilador: "essa variavel existe, mas a memoria
 *  esta reservada em outro arquivo (sessao.c)". Sem extern, terhamos uma
 *  copia em cada arquivo - bug grave de duplicacao.
 */
extern Sessao sessoes[MAX_SESSOES];
extern int    total_sessoes;
extern int    proximo_id;

/*
 *  Relogio simulado global. Comeca em 09:00 quando o sistema e inicializado
 *  e avanca conforme o operador escolhe a opcao "Avancar tempo" no menu.
 *  E usado pelo modulo de tarifa para escolher o multiplicador de horario.
 */
extern int    hora_simulada;
extern int    minuto_simulada;


/* ===========================================================================
 *  FUNCOES PUBLICAS DO MODULO
 * ===========================================================================
 *
 *  Aqui declaramos as ASSINATURAS (nome + parametros + retorno) das funcoes.
 *  O corpo de cada uma esta em sessao.c.
 */

/*
 * Inicializa o array de sessoes (estado OCIOSO em todas as posicoes).
 * Deve ser chamada uma unica vez, no inicio do main.
 */
void sessao_inicializar_sistema(void);

/*
 * Cria uma nova sessao no array.
 *   placa     - string com a placa do veiculo (sera copiada internamente)
 *   modelo    - modelo do carregador HCA G2
 *   hora      - hora simulada de inicio (0 a 23)
 *   minuto    - minuto simulado de inicio (0 a 59)
 *   prioridade - 1 (normal), 2 (frota), 3 (emergencia)
 *
 * Retorna: o ID da sessao criada, ou -1 se nao houver espaco.
 */
int sessao_criar(const char *placa,
                 ModeloHCA modelo,
                 int hora,
                 int minuto,
                 int prioridade);

/*
 * Encerra uma sessao ativa. Muda o estado para FINALIZADA e congela
 * a potencia em 0. Os dados continuam no array para o relatorio.
 *
 * Retorna: 1 se encontrou e encerrou, 0 se o ID nao existe.
 */
int sessao_encerrar(int id);

/*
 * Imprime na tela a lista de todas as sessoes (ativas e finalizadas)
 * num formato tabular legivel.
 */
void sessao_listar(void);

/*
 * Imprime na tela os detalhes completos de uma sessao especifica.
 * Retorna 1 se achou, 0 se nao.
 */
int sessao_imprimir_detalhe(int id);

/*
 * Avanca a simulacao em "minutos" e atualiza, para cada sessao que esteja
 * carregando, o kwh_entregue e o custo_acumulado. Esta e a "passagem de
 * tempo" do simulador.
 */
void sessao_avancar_tempo(int minutos);

/*
 * Retorna a potencia nominal (kW) de um determinado modelo HCA.
 * Funcao auxiliar usada em varios pontos do sistema.
 */
float potencia_do_modelo(ModeloHCA modelo);

/*
 * Devolve uma string legivel (ex: "CARREGANDO") para um estado.
 * Util para impressao e logs.
 */
const char *nome_estado(EstadoSessao e);

/*
 * Devolve a string do modelo (ex: "GW11K-HCA-20") para impressao.
 */
const char *nome_modelo(ModeloHCA m);

/*
 * Soma a potencia atual de todas as sessoes em estado CARREGANDO.
 * Usada pelo modulo de demanda para saber a ocupacao do edificio.
 */
float demanda_total_atual(void);


#endif /* SESSAO_H */
