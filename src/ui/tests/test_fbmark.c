/* test_fbmark.c — a etiqueta por cima de quem desenha direto na tela (core/fbmark.h): a mistura
 * RGB565, o recorte na região desenhada e o aviso de quando a marca aparece, muda ou some (com a
 * área a refazer, a marca antiga e a nova juntas).
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define FBMARK_PATH "build/test/fbmark.bin"
#include "core/fbmark.h"
#include <unistd.h>

static int fails;

static void expect(const char *what, long got, long want)
{
    if(got != want) { printf("FALHOU %s: %ld, esperado %ld\n", what, got, want); fails++; }
}

/* grava uma marca w×h em (x, y), toda com a cor c e o alfa a */
static void write_mark(int x, int y, int w, int h, uint16_t c, uint8_t a)
{
    FILE *f = fopen(FBMARK_PATH ".tmp", "wb");
    if(!f) { perror(FBMARK_PATH); exit(1); }
    uint16_t hd[4] = { (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h };
    fwrite("BFM1", 1, 4, f);
    fwrite(hd, sizeof hd[0], 4, f);
    for(int i = 0; i < w * h; i++) fwrite(&c, sizeof c, 1, f);
    for(int i = 0; i < w * h; i++) fwrite(&a, 1, 1, f);
    fclose(f);
    rename(FBMARK_PATH ".tmp", FBMARK_PATH);
}

static int poll_now(fbmark_t *m)
{
    m->next_ms = 0;                                     /* sem esperar os 300 ms */
    return fbmark_poll(m);
}

int main(void)
{
    static fbmark_t m;
    static uint16_t fb[20 * 10];
    unlink(FBMARK_PATH);

    /* mistura: os extremos e o meio, nos três canais */
    expect("alfa 0", fbmark_blend(0x1234, 0xFFFF, 0), 0x1234);
    expect("alfa 32", fbmark_blend(0x1234, 0xFFFF, 32), 0xFFFF);
    expect("meio branco sobre preto", fbmark_blend(0x0000, 0xFFFF, 16), 0x7BEF);

    /* sem arquivo: nada muda */
    expect("sem marca", poll_now(&m), 0);
    expect("sem marca ok", m.ok, 0);

    /* apareceu: a área a refazer é a dela */
    write_mark(4, 2, 3, 2, 0xFFFF, 32);
    expect("apareceu", poll_now(&m), 1);
    expect("apareceu rx", m.rx, 4);
    expect("apareceu ry", m.ry, 2);
    expect("apareceu rw", m.rw, 3);
    expect("apareceu rh", m.rh, 2);
    expect("mesmo arquivo", poll_now(&m), 0);
    expect("dentro do prazo", fbmark_poll(&m), 0);

    /* aplicar só na região desenhada: as colunas 4..5 sim, a 6 fica fora */
    fbmark_apply(&m, fb, 20, 20, 10, 0, 0, 6, 10);
    expect("pintou dentro", fb[2 * 20 + 4], 0xFFFF);
    expect("pintou dentro 2", fb[3 * 20 + 5], 0xFFFF);
    expect("fora da região", fb[2 * 20 + 6], 0);
    expect("fora da marca", fb[4 * 20 + 4], 0);

    /* mudou de lugar: a área junta a antiga e a nova */
    usleep(20000);                                      /* outro mtime */
    write_mark(10, 5, 2, 2, 0xF800, 16);
    expect("mudou", poll_now(&m), 1);
    expect("mudou rx", m.rx, 4);
    expect("mudou ry", m.ry, 2);
    expect("mudou rw", m.rw, 8);
    expect("mudou rh", m.rh, 5);

    /* sumiu: a área é a antiga */
    unlink(FBMARK_PATH);
    expect("sumiu", poll_now(&m), 1);
    expect("sumiu ok", m.ok, 0);
    expect("sumiu rx", m.rx, 10);
    expect("sumiu rw", m.rw, 2);
    expect("continua sumida", poll_now(&m), 0);

    /* arquivo inválido: não carrega e não avisa à toa */
    FILE *f = fopen(FBMARK_PATH, "wb");
    fputs("XXXX", f);
    fclose(f);
    expect("inválido", poll_now(&m), 0);
    expect("inválido ok", m.ok, 0);
    unlink(FBMARK_PATH);

    if(fails) return 1;
    puts("fbmark: ok");
    return 0;
}
