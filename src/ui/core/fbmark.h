/* fbmark.h — a etiqueta "Modo performance" (marca d'água) por cima de quem desenha direto na
 * tela: o player da TV (bftvd), o motor do Big Remote Play (bfrp) e o vídeo em tela cheia do
 * Internet (shell). O LVGL não desenha por cima deles; quem desenha mistura a marca depois de
 * cada quadro, só nos pontos que ela cobre.
 *
 * O shell (core/perfui.c) grava a etiqueta já com a transparência em FBMARK_PATH enquanto o
 * modo performance estiver ligado e apaga o arquivo quando ele desliga:
 *
 *   "BFM1" | u16 x, y, w, h | w*h u16 RGB565 | w*h u8 alfa 0..32       (ordem do aparelho)
 *
 * FBMARK_HOLD existe enquanto um diálogo está aberto por cima da TV: o player não desenha (o
 * som e a decodificação seguem), senão o quadro seguinte apagaria o diálogo.
 *
 * Só cabeçalho (static inline): cada programa compila o seu. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BF_FBMARK_H
#define BF_FBMARK_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define FBMARK_PATH "/run/bigfoston-mark"
#define FBMARK_HOLD "/run/bigfoston-tv-hold"
#define FBMARK_MAX_W 400
#define FBMARK_MAX_H 64

typedef struct {
    int ok, x, y, w, h;
    uint16_t px[FBMARK_MAX_W * FBMARK_MAX_H];
    uint8_t a[FBMARK_MAX_W * FBMARK_MAX_H];
    struct timespec mt;           /* mtime do arquivo carregado */
    long next_ms;                 /* próxima conferência do arquivo */
} fbmark_t;

static inline long fbmark_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

/* confere o arquivo no máximo a cada 300 ms (stat em tmpfs) e recarrega se mudou */
static inline void fbmark_poll(fbmark_t *m)
{
    long now = fbmark_now_ms();
    if(now < m->next_ms) return;
    m->next_ms = now + 300;
    struct stat st;
    if(stat(FBMARK_PATH, &st)) { m->ok = 0; return; }
    if(m->ok && st.st_mtim.tv_sec == m->mt.tv_sec && st.st_mtim.tv_nsec == m->mt.tv_nsec) return;
    m->ok = 0;
    FILE *f = fopen(FBMARK_PATH, "rb");
    if(!f) return;
    char magic[4];
    uint16_t hd[4];
    if(fread(magic, 1, 4, f) == 4 && !memcmp(magic, "BFM1", 4) && fread(hd, sizeof hd[0], 4, f) == 4 &&
       hd[2] > 0 && hd[3] > 0 && hd[2] <= FBMARK_MAX_W && hd[3] <= FBMARK_MAX_H) {
        size_t n = (size_t)hd[2] * hd[3];
        if(fread(m->px, sizeof m->px[0], n, f) == n && fread(m->a, 1, n, f) == n) {
            m->x = hd[0]; m->y = hd[1]; m->w = hd[2]; m->h = hd[3];
            m->mt = st.st_mtim;
            m->ok = 1;
        }
    }
    fclose(f);
}

/* src sobre dst com alfa 0..32, os três canais do RGB565 de uma vez */
static inline uint16_t fbmark_blend(uint16_t dst, uint16_t src, unsigned a)
{
    uint32_t d = (dst | (uint32_t)dst << 16) & 0x07E0F81Fu, s = (src | (uint32_t)src << 16) & 0x07E0F81Fu;
    uint32_t r = ((s * a + d * (32 - a)) >> 5) & 0x07E0F81Fu;
    return (uint16_t)(r | r >> 16);
}

/* mistura a marca na parte do retângulo (rx, ry, rw, rh) que acabou de ser desenhada */
static inline void fbmark_apply(const fbmark_t *m, uint16_t *fb, int stride, int fb_w, int fb_h, int rx, int ry, int rw,
                                int rh)
{
    if(!m->ok || !fb) return;
    int x0 = m->x > rx ? m->x : rx, y0 = m->y > ry ? m->y : ry;
    int x1 = m->x + m->w < rx + rw ? m->x + m->w : rx + rw, y1 = m->y + m->h < ry + rh ? m->y + m->h : ry + rh;
    if(x1 > fb_w) x1 = fb_w;
    if(y1 > fb_h) y1 = fb_h;
    if(x0 < 0) x0 = 0;
    if(y0 < 0) y0 = 0;
    for(int y = y0; y < y1; y++) {
        const uint16_t *sp = m->px + (size_t)(y - m->y) * m->w;
        const uint8_t *ap = m->a + (size_t)(y - m->y) * m->w;
        uint16_t *d = fb + (size_t)y * stride;
        for(int x = x0; x < x1; x++) {
            unsigned a = ap[x - m->x];
            if(a) d[x] = a >= 32 ? sp[x - m->x] : fbmark_blend(d[x], sp[x - m->x], a);   /* só onde há marca */
        }
    }
}

/* um diálogo por cima: não desenhar (conferido no máximo a cada 100 ms) */
static inline int fbmark_hold(void)
{
    static long next;
    static int held;
    long now = fbmark_now_ms();
    if(now >= next) {
        struct stat st;
        held = stat(FBMARK_HOLD, &st) == 0;
        next = now + 100;
    }
    return held;
}
#endif
