/* perfpolicy.h — as regras dos modos TV e GPS (plano de eficiência), sem LVGL nem estado: o que
 * pausar e o que segurar, para o perfui (core/perfui.c) aplicar e o teste conferir
 * (tests/test_perfpolicy.c). Especificação: docs/modos-tv-gps/02-sistema-performance-energia-eficiencia.md
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef BF_PERFPOLICY_H
#define BF_PERFPOLICY_H
#include <stdbool.h>

typedef enum { PERF_MODE_NONE, PERF_MODE_TV, PERF_MODE_GPS } perf_mode_t;

/* grupos PM_SVC_* (perfmode.h) que o modo pede para pausar; 0 sem modo (vale "O que pausar") */
unsigned perfpolicy_pause(perf_mode_t m);

/* o que fica rodando mesmo com o modo pedindo para pausar:
 *   app_needs   o que o app aberto usa
 *   bt_conn     algum aparelho Bluetooth conectado (celular, fone, teclado, controle)
 *   phone_conn  o celular conectado (as chamadas precisam chegar: telefone e Bluetooth)
 *   in_grace    começo do modo (carência): o que ainda roda não pausa já
 *   paused      o que já está pausado (a carência não religa nada) */
unsigned perfpolicy_holds(perf_mode_t m, unsigned app_needs, bool bt_conn, bool phone_conn, bool in_grace,
                          unsigned paused);
#endif
