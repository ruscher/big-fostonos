/* test_perfpolicy.c — as regras dos modos TV e GPS (core/perfpolicy.c): o que pausa e o que fica,
 * em todas as combinações de conexão, carência e app aberto.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/perfpolicy.h"
#include "core/perfmode.h"
#include <stdio.h>
#include <stdlib.h>

static int fails;
#define ALL (PM_SVC_GPS | PM_SVC_BT | PM_SVC_PHONE | PM_SVC_CRON)

/* o que de fato para: o pedido menos o que fica (como o perfmode faz: groups & ~held) */
static unsigned stops(perf_mode_t m, unsigned app, int bt, int ph, int grace, unsigned paused)
{
    return perfpolicy_pause(m) & ~perfpolicy_holds(m, app, bt, ph, grace, paused);
}

static void expect(const char *what, unsigned got, unsigned want)
{
    if(got != want) { printf("FALHOU %s: 0x%x, esperado 0x%x\n", what, got, want); fails++; }
}

int main(void)
{
    /* sem modo: nada pedido aqui (vale "O que pausar" do usuário) */
    expect("sem modo", perfpolicy_pause(PERF_MODE_NONE), 0);

    /* TV, nada conectado, depois da carência: GPS, tarefas, Bluetooth e telefone */
    expect("TV livre", stops(PERF_MODE_TV, 0, 0, 0, 0, 0), ALL);
    /* TV com o celular conectado: só GPS e tarefas (chamadas e Bluetooth continuam) */
    expect("TV celular", stops(PERF_MODE_TV, 0, 1, 1, 0, 0), PM_SVC_GPS | PM_SVC_CRON);
    /* TV com só um fone/teclado/controle: o Bluetooth fica, o telefone pausa */
    expect("TV fone", stops(PERF_MODE_TV, 0, 1, 0, 0, 0), PM_SVC_GPS | PM_SVC_CRON | PM_SVC_PHONE);
    /* o canal de chamadas aberto sem o BlueZ ver o aparelho ainda: os dois ficam */
    expect("TV só link", stops(PERF_MODE_TV, 0, 0, 1, 0, 0), PM_SVC_GPS | PM_SVC_CRON);
    /* TV na carência, nada conectado: Bluetooth e telefone ainda não pausam */
    expect("TV carência", stops(PERF_MODE_TV, 0, 0, 0, 1, 0), PM_SVC_GPS | PM_SVC_CRON);
    /* carência com o Bluetooth já pausado antes: não o religa (continua parado) */
    expect("TV carência pausado", perfpolicy_holds(PERF_MODE_TV, 0, 0, 0, 1, PM_SVC_BT) & PM_SVC_BT, 0);
    expect("TV carência pausado (telefone fica)", perfpolicy_holds(PERF_MODE_TV, 0, 0, 0, 1, PM_SVC_BT) & PM_SVC_PHONE,
           PM_SVC_PHONE);

    /* GPS, nada conectado: tarefas, Bluetooth e telefone; o GPS o mapa segura */
    expect("GPS livre", stops(PERF_MODE_GPS, PM_SVC_GPS, 0, 0, 0, 0), PM_SVC_CRON | PM_SVC_BT | PM_SVC_PHONE);
    expect("GPS nunca pausa o GPS", perfpolicy_pause(PERF_MODE_GPS) & PM_SVC_GPS, 0);
    /* GPS com o celular: só as tarefas */
    expect("GPS celular", stops(PERF_MODE_GPS, PM_SVC_GPS, 1, 1, 0, 0), PM_SVC_CRON);
    /* GPS na carência: só as tarefas */
    expect("GPS carência", stops(PERF_MODE_GPS, PM_SVC_GPS, 0, 0, 1, 0), PM_SVC_CRON);

    /* sem modo, a carência não vale (nada é segurado por ela) */
    expect("sem modo sem carência", perfpolicy_holds(PERF_MODE_NONE, 0, 0, 0, 1, 0), 0);
    /* o app aberto sempre fica (Telefone aberto durante o modo) */
    expect("app segura", perfpolicy_holds(PERF_MODE_TV, PM_SVC_PHONE | PM_SVC_BT, 0, 0, 0, 0), PM_SVC_PHONE | PM_SVC_BT);

    /* exaustivo: nunca pausa o que está conectado, em modo nenhum */
    for(int m = PERF_MODE_NONE; m <= PERF_MODE_GPS; m++)
        for(int bt = 0; bt < 2; bt++)
            for(int ph = 0; ph < 2; ph++)
                for(int g = 0; g < 2; g++)
                    for(unsigned paused = 0; paused <= ALL; paused++) {
                        unsigned s = stops((perf_mode_t)m, 0, bt, ph, g, paused);
                        if(bt && (s & PM_SVC_BT)) { printf("FALHOU BT conectado pausado (m%d)\n", m); fails++; }
                        if(ph && (s & (PM_SVC_PHONE | PM_SVC_BT))) { printf("FALHOU celular conectado pausado (m%d)\n", m); fails++; }
                        if(m == PERF_MODE_GPS && (s & PM_SVC_GPS)) { printf("FALHOU GPS pausado no modo GPS\n"); fails++; }
                    }
    if(fails) { printf("%d falha(s)\n", fails); return 1; }
    printf("ok\n");
    return 0;
}
