/*
 * ============================================================================
 *  tarifa.c  -  Implementacao da tarifacao dinamica
 * ============================================================================
 */

#include <stdio.h>
#include "tarifa.h"
#include "sessao.h"

/* ===========================================================================
 *  MULTIPLICADORES - cada funcao retorna o fator a aplicar sobre TARIFA_BASE
 * ===========================================================================
 *
 *  Os tres fatores sao multiplicados entre si, entao um valor de 1.0 e neutro
 *  (nao altera a tarifa), 1.5 aumenta em 50%, 0.7 reduz em 30%.
 */

/*
 *  multiplicador_horario  -  modelo inspirado na tarifa branca da ANEEL.
 *
 *      Ponta             18h - 20h59   -> 1.5  (mais caro)
 *      Intermediario     17h ou 21h    -> 1.2  (transicao)
 *      Fora de ponta     22h - 05h59   -> 0.7  (mais barato, incentiva recarga noturna)
 *      Padrao            demais horas  -> 1.0  (referencia)
 */
static float multiplicador_horario(int hora) {
    if (hora >= 18 && hora <= 20) return 1.5f;
    if (hora == 17 || hora == 21) return 1.2f;
    if (hora >= 22 || hora <= 5)  return 0.7f;
    return 1.0f;
}

/*
 *  multiplicador_demanda  -  surge pricing baseado em ocupacao do edificio.
 *
 *  A ideia: quando a demanda contratada esta perto do limite, queremos
 *  desencorajar novas sessoes (e cobrar mais das que ja estao rodando)
 *  para evitar que o sistema entre em controle de demanda agressivo.
 *
 *      Ocupacao acima de 80%    -> 1.3  (pico de demanda)
 *      Ocupacao entre 50% e 80% -> 1.1  (moderada)
 *      Ocupacao abaixo de 50%   -> 1.0  (folga, sem sobretaxa)
 */
static float multiplicador_demanda(float demanda_atual, float demanda_contratada) {
    /* defesa contra divisao por zero - nunca deveria acontecer mas vale a paranoia */
    if (demanda_contratada <= 0.0f) return 1.0f;

    float ocupacao = demanda_atual / demanda_contratada;

    if (ocupacao > 0.8f) return 1.3f;
    if (ocupacao > 0.5f) return 1.1f;
    return 1.0f;
}

/*
 *  multiplicador_modelo  -  premium por velocidade de recarga.
 *
 *  A logica comercial: o GW22K-HCA-20 entrega energia em 1/3 do tempo
 *  do GW7K-HCA-20. O cliente paga premium pelo "tempo economizado".
 *
 *      GW22K (rapido)    -> 1.20
 *      GW11K (mediano)   -> 1.10
 *      GW7K  (padrao)    -> 1.00
 */
static float multiplicador_modelo(ModeloHCA modelo) {
    switch (modelo) {
        case GW7K:  return 1.00f;
        case GW11K: return 1.10f;
        case GW22K: return 1.20f;
    }
    return 1.0f;
}


/* ===========================================================================
 *  FUNCOES PUBLICAS
 * ===========================================================================
 */

float tarifa_calcular(int hora,
                      float demanda_atual,
                      float demanda_contratada,
                      ModeloHCA modelo) {
    float mh = multiplicador_horario(hora);
    float md = multiplicador_demanda(demanda_atual, demanda_contratada);
    float mm = multiplicador_modelo(modelo);

    return TARIFA_BASE * mh * md * mm;
}


void tarifa_recalcular_sessoes(void) {
    float demanda_agora = demanda_total_atual();
    int   ajustadas    = 0;

    for (int i = 0; i < total_sessoes; i++) {
        Sessao *s = &sessoes[i];

        if (s->estado == CARREGANDO || s->estado == PAUSADA_DEMANDA) {
            float tarifa = tarifa_calcular(
                hora_simulada,
                demanda_agora,
                DEMANDA_CONTRATADA,
                s->modelo
            );

            /*
             *  Recalculamos do zero: custo = kWh ja entregues x tarifa atual.
             *  Esse "recompute" e proposital - quando o operador chama esse
             *  comando ele esta dizendo "aplica a tarifa de AGORA sobre tudo
             *  que ja foi entregue".
             */
            s->custo_acumulado = s->kwh_entregue * tarifa;
            ajustadas++;
        }
    }

    printf("[tarifa] %d sessao(oes) tiveram seu custo recalculado com a tarifa atual.\n",
           ajustadas);
}


const char *nome_faixa_horaria(int hora) {
    if (hora >= 18 && hora <= 20) return "PONTA";
    if (hora == 17 || hora == 21) return "INTERMEDIARIO";
    if (hora >= 22 || hora <= 5)  return "FORA DE PONTA";
    return "PADRAO";
}


const char *nome_faixa_ocupacao(float demanda_atual, float demanda_contratada) {
    if (demanda_contratada <= 0.0f) return "INDEFINIDO";
    float ocupacao = demanda_atual / demanda_contratada;
    if (ocupacao > 0.8f) return "PICO";
    if (ocupacao > 0.5f) return "MODERADA";
    return "FOLGA";
}


void tarifa_explicar(void) {
    float demanda_agora = demanda_total_atual();
    float ocupacao_pct  = (DEMANDA_CONTRATADA > 0.0f)
                          ? (demanda_agora / DEMANDA_CONTRATADA) * 100.0f
                          : 0.0f;

    float mh = multiplicador_horario(hora_simulada);
    float md = multiplicador_demanda(demanda_agora, DEMANDA_CONTRATADA);

    printf("\n");
    printf("  =================================================================\n");
    printf("    Decomposicao da tarifa atual\n");
    printf("  =================================================================\n");
    printf("    Tarifa base..................... R$ %.2f / kWh\n", TARIFA_BASE);
    printf("    Hora simulada................... %02d:%02d (%s)\n",
           hora_simulada, minuto_simulada, nome_faixa_horaria(hora_simulada));
    printf("    Multiplicador de horario........ x %.2f\n", mh);
    printf("    Demanda atual / contratada...... %.2f / %.1f kW (%.1f%% - %s)\n",
           demanda_agora, (float)DEMANDA_CONTRATADA, ocupacao_pct,
           nome_faixa_ocupacao(demanda_agora, DEMANDA_CONTRATADA));
    printf("    Multiplicador de demanda........ x %.2f\n", md);
    printf("  -----------------------------------------------------------------\n");
    printf("    Tarifa final por modelo HCA G2:\n");
    printf("      GW7K-HCA-20  (1.00x).......... R$ %.3f / kWh\n",
           tarifa_calcular(hora_simulada, demanda_agora, DEMANDA_CONTRATADA, GW7K));
    printf("      GW11K-HCA-20 (1.10x).......... R$ %.3f / kWh\n",
           tarifa_calcular(hora_simulada, demanda_agora, DEMANDA_CONTRATADA, GW11K));
    printf("      GW22K-HCA-20 (1.20x).......... R$ %.3f / kWh\n",
           tarifa_calcular(hora_simulada, demanda_agora, DEMANDA_CONTRATADA, GW22K));
    printf("  =================================================================\n\n");
}
