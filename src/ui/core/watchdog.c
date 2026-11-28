/* watchdog.c — vigia do shell travado (watchdog.h).
 *
 * O S90bigfoston-shell reinicia o shell que sai, não o que trava: com o laço principal parado a
 * tela congela e, no carro, só desligando. O laço chama watchdog_beat() a cada volta (no máximo
 * 1 s entre elas, main.c); uma thread acorda a cada WD_STEP_S e, sem nenhuma volta em limit_s,
 * registra o motivo e sai. Sem processo novo e sem arquivo: a thread só lê um contador.
 *
 * Os filhos que desenham na tela (bftvd) saem junto (PR_SET_PDEATHSIG em scr_tv.c).
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "watchdog.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define WD_STEP_S 5

static atomic_uint beats;
static unsigned limit;

void watchdog_beat(void) { atomic_fetch_add_explicit(&beats, 1, memory_order_relaxed); }

static void say(const char *path, const char *msg)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if(fd < 0) return;
    ssize_t r = write(fd, msg, strlen(msg));
    (void)r;
    close(fd);
}

static void *run(void *arg)
{
    (void)arg;
    unsigned last = atomic_load(&beats), still = 0;
    for(;;) {
        struct timespec ts = { WD_STEP_S, 0 };
        while(nanosleep(&ts, &ts) != 0) {}
        unsigned b = atomic_load(&beats);
        if(b != last) { last = b; still = 0; continue; }
        still += WD_STEP_S;
        if(still < limit) continue;
        char msg[128];
        snprintf(msg, sizeof msg, "bigfoston-shell: laço parado há %u s, reiniciando\n", still);
        ssize_t r = write(2, msg, strlen(msg));          /* /var/log/bigfoston-shell.log */
        (void)r;
        say("/dev/kmsg", msg);
        _exit(WATCHDOG_EXIT);                             /* sem atexit: o laço pode estar no meio de algo */
    }
    return NULL;
}

void watchdog_start(unsigned limit_s)
{
    const char *off = getenv("BIGFOSTON_NO_WATCHDOG");
    if(limit || (off && *off && strcmp(off, "0"))) return;
    limit = limit_s < WD_STEP_S ? WD_STEP_S : limit_s;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 64 * 1024);
    pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    if(pthread_create(&t, &a, run, NULL)) limit = 0;
    pthread_attr_destroy(&a);
}
