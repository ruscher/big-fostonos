/* perfpolicy.c — ver perfpolicy.h.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "perfpolicy.h"
#include "perfmode.h"

unsigned perfpolicy_pause(perf_mode_t m)
{
    switch(m) {
    /* TV: o GPS, as tarefas agendadas e, se nada estiver conectado, Bluetooth e telefone */
    case PERF_MODE_TV: return PM_SVC_GPS | PM_SVC_CRON | PM_SVC_BT | PM_SVC_PHONE;
    /* GPS: as tarefas e, se nada estiver conectado, Bluetooth e telefone (o GPS o app segura) */
    case PERF_MODE_GPS: return PM_SVC_CRON | PM_SVC_BT | PM_SVC_PHONE;
    default: return 0;
    }
}

unsigned perfpolicy_holds(perf_mode_t m, unsigned app_needs, bool bt_conn, bool phone_conn, bool in_grace,
                          unsigned paused)
{
    unsigned h = app_needs;
    /* um aparelho Bluetooth conectado nunca cai: pausar derrubava o celular (e a internet e o som
     * dele), o teclado e o controle */
    if(bt_conn) h |= PM_SVC_BT;
    /* o celular conectado: as chamadas chegam pelo bfphone, que fala com ele pelo Bluetooth */
    if(phone_conn) h |= PM_SVC_PHONE | PM_SVC_BT;
    /* carência do modo: quem acabou de entrar no carro ainda vai conectar o celular */
    if(m != PERF_MODE_NONE && in_grace) h |= (PM_SVC_BT | PM_SVC_PHONE) & ~paused;
    return h;
}
