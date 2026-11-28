/* padhold.h — aperto e intervalo mínimos dos botões do controle (bfrp.c, send_pad).
 *
 * O aperto e a soltura saem do FOSTON com o intervalo certo, mas podem chegar juntos ao relay
 * (o FOSTON ou o relay ocupado com o vídeo lê várias linhas de uma vez). Medido em 28/11/2026:
 * B apertado 81-232 ms no FOSTON chegava ao Sunshine com 6 e 10 ms, e B apertado de novo 10 ms
 * depois de soltar chegava como um aperto só; o jogo, que lê o controle a cada 16,7 ms, perdia
 * o botão. Aqui cada botão fica pelo menos PADHOLD_MIN_MS apertado e PADHOLD_MIN_MS solto (duas
 * leituras a 60 qps), e um aperto que chega durante a espera é guardado, nunca perdido.
 * Só cabeçalho, com o relógio de fora: testado em src/ui/tests/test_padhold.c.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BFRP_PADHOLD_H
#define BFRP_PADHOLD_H
#include <stdbool.h>
#include <stdint.h>

#define PADHOLD_MIN_MS 34
#define PADHOLD_NBTN   16

typedef struct {
    int sent;                            /* botões que o Sunshine tem como apertados */
    int latch;                           /* apertos que ainda esperam o intervalo solto */
    int64_t down_at[PADHOLD_NBTN], up_at[PADHOLD_NBTN];
} padhold_t;

/* sem sessão: nada apertado e nada guardado para a próxima */
static inline void padhold_reset(padhold_t *h) { h->sent = h->latch = 0; }

/* o que mandar agora para os botões do controle (buttons) no instante now (ms, monotônico);
 * *wait: ms até a próxima mudança adiada, ou -1 sem nenhuma */
static inline int padhold_step(padhold_t *h, int buttons, int64_t now, int *wait)
{
    int eff = buttons, w = -1;
    h->latch |= buttons & ~h->sent;                  /* um aperto nunca se perde, mesmo já solto */
    for(int i = 0; i < PADHOLD_NBTN; i++) {
        int f = 1 << i;
        bool was = h->sent & f, want = (buttons & f) || (!was && (h->latch & f));
        if(want) eff |= f;
        if(want == was) {
            if(was) h->latch &= ~f;
            continue;
        }
        int64_t left = (want ? h->up_at[i] : h->down_at[i]) + PADHOLD_MIN_MS - now;
        if(left > 0) {                               /* ainda não completou: fica como está */
            eff = (eff & ~f) | (was ? f : 0);
            if(w < 0 || left < w) w = (int)left;
        } else if(want) {
            h->down_at[i] = now;
            h->latch &= ~f;
        } else h->up_at[i] = now;
    }
    /* enviado diferente do controle (um aperto guardado já solto): soltar depois do mínimo */
    if(eff != buttons && w < 0) w = PADHOLD_MIN_MS;
    h->sent = eff;
    *wait = w;
    return eff;
}
#endif
