/* padhold.h — aperto e intervalo mínimos dos botões do controle (bfrp.c, send_pad).
 *
 * O aperto e a soltura saem do FOSTON com o intervalo certo, mas podem chegar juntos ao relay
 * (o FOSTON ou o relay ocupado com o vídeo lê várias linhas de uma vez). Medido em 28/11/2026:
 * B apertado 81-232 ms no FOSTON chegava ao Sunshine com 6 e 10 ms, e B apertado de novo 10 ms
 * depois de soltar chegava como um aperto só; o jogo, que lê o controle a cada 16,7 ms, perdia
 * o botão.
 *
 * Cada botão tem uma fila de trocas: toda mudança que chega entra na fila e sai na ordem, uma de
 * cada vez, com o botão pelo menos PADHOLD_MIN_MS em cada estado (duas leituras a 60 qps). Um
 * aperto longo isolado passa sem atraso; apertar e soltar várias vezes de uma vez sai como
 * vários apertos, espaçados. Só cabeçalho, com o relógio de fora: src/ui/tests/test_padhold.c.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BFRP_PADHOLD_H
#define BFRP_PADHOLD_H
#include <stdint.h>

#define PADHOLD_MIN_MS 34
#define PADHOLD_NBTN   16
#define PADHOLD_MAXQ   8                 /* trocas na fila por botão (par: o último estado vale) */

typedef struct {
    int sent;                            /* botões que o Sunshine tem como apertados */
    int seen;                            /* o último estado do controle recebido */
    uint8_t queue[PADHOLD_NBTN];         /* trocas recebidas e ainda não mandadas */
    int64_t at[PADHOLD_NBTN];            /* quando o botão mudou no Sunshine pela última vez */
} padhold_t;

/* sem sessão: nada apertado e nada na fila para a próxima */
static inline void padhold_reset(padhold_t *h)
{
    h->sent = h->seen = 0;
    for(int i = 0; i < PADHOLD_NBTN; i++) h->queue[i] = 0;
}

/* o que mandar agora para os botões do controle (buttons) no instante now (ms, monotônico);
 * *wait: ms até a próxima troca da fila, ou -1 sem nenhuma */
static inline int padhold_step(padhold_t *h, int buttons, int64_t now, int *wait)
{
    int w = -1;
    for(int i = 0; i < PADHOLD_NBTN; i++) {
        int f = 1 << i;
        if((buttons ^ h->seen) & f) {                /* uma troca nova entra na fila */
            h->queue[i]++;
            if(h->queue[i] > PADHOLD_MAXQ) h->queue[i] -= 2;   /* cheia: perde um aperto inteiro */
        }
        if(!h->queue[i]) continue;
        int64_t left = h->at[i] + PADHOLD_MIN_MS - now;
        if(left <= 0) {                              /* o estado atual já durou o mínimo: troca */
            h->sent ^= f;
            h->at[i] = now;
            h->queue[i]--;
            left = PADHOLD_MIN_MS;
        }
        if(h->queue[i] && (w < 0 || left < w)) w = (int)left;
    }
    h->seen = buttons;
    *wait = w;
    return h->sent;
}
#endif
