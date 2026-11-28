/* tapqueue.h — as trocas de um botão entre duas leituras, sem perder nenhuma
 * (gamepad_buttons_take, gamepad.c).
 *
 * Quem lê o controle de tempos em tempos (o Big Remote Play, a cada 8 ms) via só o estado da
 * hora: um apertar-e-soltar inteiro entre duas leituras sumia. Cada troca entra numa fila e cada
 * leitura entrega a próxima, na ordem; o que não foi entregue fica para a leitura seguinte.
 * Testado em tests/test_padhold.c.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BF_TAPQUEUE_H
#define BF_TAPQUEUE_H
#include <stdbool.h>
#include <stdint.h>

#define TAPQ_MAX 8                       /* trocas guardadas (par: o último estado vale) */

/* o botão trocou de estado */
static inline void tapq_change(uint8_t *q)
{
    if(++*q > TAPQ_MAX) *q -= 2;                     /* cheia: perde um toque inteiro */
}

/* o estado a entregar nesta leitura: a próxima troca da fila, ou o mesmo de antes */
static inline bool tapq_take(bool *taken, uint8_t *q)
{
    if(*q) { *taken = !*taken; (*q)--; }
    return *taken;
}
#endif
