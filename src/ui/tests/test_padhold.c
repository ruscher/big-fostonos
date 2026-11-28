/* test_padhold.c — aperto e intervalo mínimos dos botões do controle no Big Remote Play
 * (remoteplay/padhold.h): o que o Sunshine recebe quando o aperto e a soltura chegam juntos.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../remoteplay/padhold.h"
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

    if(fails) return 1;
    puts("padhold: ok");
    return 0;
}
