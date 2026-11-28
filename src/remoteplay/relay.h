/* relay.h — o bfrp no PC (relay.c) e o cliente no FOSTON (client.c).
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BFRP_RELAY_H
#define BFRP_RELAY_H
#include <stdint.h>

/* PC */
void relay_attach(int fd);                               /* o FOSTON conectado (-1: nenhum) */
int  relay_send(char type, const void *data, uint32_t len);
void relay_ack(void);                                    /* o FOSTON desenhou o quadro */
void relay_key(void);                                    /* o próximo quadro vai inteiro */
void relay_set_target(int w);                            /* largura do quadro enviado */
void relay_stats(unsigned long *decoded, unsigned long *sent, unsigned long long *bytes);

/* FOSTON: espera o relay e faz a ponte stdin/stdout <-> relay; se ele cair, espera de novo.
 * 0 = a tela saiu, -1 = nenhum relay no começo (o motor segue no modo direto) */
int  relay_client_main(const char *host, int port);
#endif
