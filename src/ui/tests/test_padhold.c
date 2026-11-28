/* test_padhold.c — os botões do controle no Big Remote Play sem toque perdido: o aperto e o
 * intervalo mínimos do relay (remoteplay/padhold.h), com o que o Sunshine recebe quando as
 * trocas chegam juntas, e a fila de trocas da tela entre duas leituras (core/tapqueue.h).
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../remoteplay/padhold.h"
#include "core/tapqueue.h"
#include <stdio.h>
#include <string.h>

#define B 2                                             /* um botão qualquer (bit 1) */

static int fails;

static void expect(const char *what, long got, long want)
{
    if(got != want) { printf("FALHOU %s: %ld, esperado %ld\n", what, got, want); fails++; }
}

/* um evento do controle: no instante t (ms), os botões passam a ser buttons */
typedef struct { int t, buttons; } ev_t;

/* roda os eventos ms a ms (como o laço do relay chamando pad_tick) e anota, em ordem, os instantes
 * em que o B mudou no Sunshine: positivo = apertou, negativo = soltou */
static int run(const ev_t *ev, int n, int out[], int max)
{
    static padhold_t h;
    memset(&h, 0, sizeof h);
    int k = 0, sent = 0, buttons = 0, wait = -1, i = 0;
    for(int t = 1000; t < 1600; t++) {                  /* começa em 1 s: o relógio real nunca é 0 */
        bool step = false;
        /* cada linha PAD passa por send_pad, mesmo as que chegam juntas */
        while(i < n && ev[i].t + 1000 == t) {
            buttons = ev[i++].buttons;
            step = true;
            int eff = padhold_step(&h, buttons, t, &wait);
            if((eff & B) != (sent & B) && k < max) out[k++] = (eff & B) ? t - 1000 + 1 : -(t - 1000 + 1);
            sent = eff;
        }
        if(step || wait < 0) continue;
        int eff = padhold_step(&h, buttons, t, &wait);  /* pad_tick: o prazo de uma mudança adiada */
        if((eff & B) != (sent & B) && k < max) out[k++] = (eff & B) ? t - 1000 + 1 : -(t - 1000 + 1);
        sent = eff;
    }
    return k;
}

int main(void)
{
    int o[8], k;

    /* aperto de 6 ms (o que chegava ao Sunshine): fica 34 ms apertado */
    k = run((ev_t[]){ { 0, B }, { 6, 0 } }, 2, o, 8);
    expect("6 ms: mudanças", k, 2);
    expect("6 ms: apertou já", o[0], 1);
    expect("6 ms: soltou aos 34", o[1], -35);

    /* aperto e soltura no mesmo instante (chegaram juntos): o aperto não some */
    k = run((ev_t[]){ { 0, B }, { 0, 0 } }, 2, o, 8);
    expect("juntos: mudanças", k, 2);
    expect("juntos: apertou", o[0], 1);
    expect("juntos: soltou aos 34", o[1], -35);

    /* B martelado, 10 ms solto (medido: o jogo via um aperto só): o segundo espera 34 ms solto */
    k = run((ev_t[]){ { 0, B }, { 100, 0 }, { 110, B }, { 220, 0 } }, 4, o, 8);
    expect("martelado: mudanças", k, 4);
    expect("martelado: soltou aos 100", o[1], -101);
    expect("martelado: apertou de novo aos 134", o[2], 135);
    expect("martelado: soltou aos 220", o[3], -221);

    /* um aperto inteiro durante a espera do intervalo solto: guardado e mandado depois */
    k = run((ev_t[]){ { 0, B }, { 50, 0 }, { 55, B }, { 60, 0 } }, 4, o, 8);
    expect("guardado: mudanças", k, 4);
    expect("guardado: apertou aos 84", o[2], 85);
    expect("guardado: soltou aos 118", o[3], -119);

    /* duas vezes apertar-e-soltar no mesmo instante: dois apertos, em ordem, 34 ms cada estado */
    k = run((ev_t[]){ { 0, B }, { 0, 0 }, { 0, B }, { 0, 0 } }, 4, o, 8);
    expect("dois juntos: mudanças", k, 4);
    expect("dois juntos: apertou", o[0], 1);
    expect("dois juntos: soltou aos 34", o[1], -35);
    expect("dois juntos: apertou aos 68", o[2], 69);
    expect("dois juntos: soltou aos 102", o[3], -103);

    /* aperto que chega com a soltura ainda na fila: não é fundido no anterior */
    k = run((ev_t[]){ { 100, B }, { 105, 0 }, { 110, B }, { 120, 0 } }, 4, o, 8);
    expect("na fila: mudanças", k, 4);
    expect("na fila: apertou aos 100", o[0], 101);
    expect("na fila: soltou aos 134", o[1], -135);
    expect("na fila: apertou aos 168", o[2], 169);
    expect("na fila: soltou aos 202", o[3], -203);

    /* aperto comum, longo: passa igual, sem atraso */
    k = run((ev_t[]){ { 0, B }, { 150, 0 } }, 2, o, 8);
    expect("comum: mudanças", k, 2);
    expect("comum: soltou aos 150", o[1], -151);

    /* sem sessão: o reset esquece o aperto guardado (não vai para a próxima sessão) */
    padhold_t h = { 0 };
    int wait;
    padhold_step(&h, B, 1000, &wait);
    padhold_step(&h, 0, 1010, &wait);
    padhold_step(&h, B, 1020, &wait);                   /* guardado: esperando o intervalo solto */
    padhold_reset(&h);
    expect("reset: nada apertado", padhold_step(&h, 0, 5000, &wait), 0);
    expect("reset: nada a esperar", wait, -1);

    /* a fila da tela: leituras a cada 8 ms, dois toques entre duas leituras */
    bool taken = false;
    uint8_t q = 0;
    tapq_change(&q); tapq_change(&q);                    /* toque 1 antes da leitura de 8 ms */
    expect("leitura 8: apertado", tapq_take(&taken, &q), 1);
    tapq_change(&q); tapq_change(&q);                    /* toque 2 aos 10-11 ms */
    expect("leitura 16: solto (fim do toque 1)", tapq_take(&taken, &q), 0);
    expect("leitura 24: apertado (toque 2)", tapq_take(&taken, &q), 1);
    expect("leitura 32: solto", tapq_take(&taken, &q), 0);
    expect("leitura 40: segue solto", tapq_take(&taken, &q), 0);

    /* segurado: soltar e apertar de novo entre duas leituras vem solto numa e apertado na outra */
    taken = false; q = 0;
    tapq_change(&q);
    expect("segurado: apertado", tapq_take(&taken, &q), 1);
    expect("segurado: continua", tapq_take(&taken, &q), 1);
    tapq_change(&q); tapq_change(&q);
    expect("segurado: solto uma vez", tapq_take(&taken, &q), 0);
    expect("segurado: apertado de novo", tapq_take(&taken, &q), 1);

    /* fila cheia: o último estado vale (perde toques inteiros, nunca fica trocado) */
    taken = false; q = 0;
    for(int i = 0; i < 21; i++) tapq_change(&q);         /* 21 trocas: termina apertado */
    bool last = false;
    for(int i = 0; i < 20; i++) last = tapq_take(&taken, &q);
    expect("cheia: termina apertado", last, 1);
    expect("cheia: fila vazia", q, 0);

    if(fails) return 1;
    puts("padhold: ok");
    return 0;
}
