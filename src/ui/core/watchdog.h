/* watchdog.h — vigia do shell travado: uma thread confere se o laço principal continua girando e,
 * parado por tempo demais, encerra o processo; o S90bigfoston-shell o reinicia.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BF_WATCHDOG_H
#define BF_WATCHDOG_H

#define WATCHDOG_LIMIT_S 45     /* laço parado por isso: travado (o mais longo medido no laço é ~1 s) */
#define WATCHDOG_EXIT    70     /* código de saída: o log do S90bigfoston-shell mostra "saiu (70)" */

void watchdog_start(unsigned limit_s);   /* BIGFOSTON_NO_WATCHDOG=1 desliga (depurador) */
void watchdog_beat(void);                /* a cada volta do laço principal */

#endif
