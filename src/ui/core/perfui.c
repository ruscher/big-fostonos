/* perfui.c — ver perfui.h.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "perfui.h"
#include "perfmode.h"
#include "fx.h"
#include "nav.h"
#include "settings.h"
#include "theme.h"
#include "i18n.h"
#include "hwkbd.h"
#include "gamepad.h"
#include "btstat.h"
#include "phonestat.h"
#include "app.h"
#include "../screens/screens.h"
#include "fbmark.h"
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#include "../widgets/dialog.h"
#include <stdio.h>
#include <string.h>

#define C_PERF 0xe66100

static bool manual, video, suppressed;   /* suppressed: desligado pelo usuário com o vídeo tocando */
static bool pad, pad_suppressed;         /* controle de jogo conectado; desligado à mão com ele */
static bool tv, tv_suppressed;           /* app TV aberto; desligado à mão com ela */
static bool stream, stream_suppressed;   /* sessão com imagem do Big Remote Play */
static bool gps, gps_suppressed;         /* Big GPS aberto; desligado à mão com ele */
static perf_mode_t mode_cur;             /* modo de uso em vigor (perfui_mode) */
static uint32_t mode_since;              /* quando o modo atual começou (carência) */
static bool on;                          /* o que foi pedido ao perfmode */
static lv_timer_t *delay_t, *tick_t;
static lv_obj_t *tag, *tag_lbl;

/* ---------------------------------------------------------------- janela de progresso */
static lv_obj_t *pm_box, *pm_bar, *pm_title, *pm_sub;
static pm_state_t pm_shown = PM_OFF;
static uint32_t pm_since;

/* apagada por fora (app_rebuild_now limpa a camada de cima ao trocar tema ou idioma) */
static void pm_deleted(lv_event_t *e)
{
    if(lv_event_get_target(e) != pm_box) return;
    pm_box = pm_bar = pm_title = pm_sub = NULL;
    pm_shown = PM_OFF;
}

static void pm_close(void)
{
    if(pm_box) lv_obj_delete(pm_box);
    pm_box = pm_bar = pm_title = pm_sub = NULL;
    pm_shown = PM_OFF;
}

static void pm_update(void)
{
    if(!pm_box) return;
    pm_state_t st = perfmode_state();
    int d, n;
    perfmode_progress(&d, &n);
    if(st == PM_ENTERING || st == PM_LEAVING) {
        if(st != pm_shown) {                     /* entrar virou sair (desligado no meio) */
            pm_shown = st;
            lv_label_set_text(pm_title, tr(st == PM_ENTERING ? S_PERF_ENTER : S_PERF_LEAVE));
            lv_label_set_text(pm_sub, tr(st == PM_ENTERING ? S_PERF_ENTER_SUB : S_PERF_LEAVE_SUB));
            pm_since = lv_tick_get();
        }
        lv_bar_set_value(pm_bar, n ? d * 100 / n : 0, LV_ANIM_ON);
        return;
    }
    lv_bar_set_value(pm_bar, 100, LV_ANIM_ON);
    if(lv_tick_elaps(pm_since) > 700) pm_close();   /* dá tempo de ler */
}

/* segura os toques durante a troca; se demorar (um serviço lento), um toque fecha a janela e
 * a troca segue por trás (a etiqueta e Ajustes mostram o estado) */
static uint32_t pm_opened;

static void pm_box_click(lv_event_t *e)
{
    (void)e;
    if(lv_tick_elaps(pm_opened) > 10000) pm_close();
}

static void pm_open(void)
{
    if(pm_box || !settings()->perf_modal) return;
    const bf_palette_t *p = pal();
    pm_box = lv_obj_create(lv_layer_top());
    lv_obj_add_event_cb(pm_box, pm_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_remove_style_all(pm_box);
    lv_obj_set_size(pm_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(pm_box, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pm_box, LV_OPA_50, 0);
    lv_obj_add_flag(pm_box, LV_OBJ_FLAG_CLICKABLE);       /* segura os toques enquanto troca */
    lv_obj_add_event_cb(pm_box, pm_box_click, LV_EVENT_CLICKED, NULL);
    pm_opened = lv_tick_get();
    lv_obj_t *card = lv_obj_create(pm_box);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 520, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, p->surface, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, BF_RADIUS_L, 0);
    lv_obj_set_style_pad_all(card, BF_SP_XL, 0);
    lv_obj_set_style_pad_row(card, BF_SP_M, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    pm_title = bf_label(card, "", BF_FONT_L, p->text);
    pm_sub = bf_label(card, "", BF_FONT_M, p->text_dim);
    lv_obj_set_width(pm_sub, LV_PCT(100));
    lv_label_set_long_mode(pm_sub, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(pm_sub, LV_TEXT_ALIGN_CENTER, 0);
    pm_bar = lv_bar_create(card);
    lv_obj_set_size(pm_bar, LV_PCT(100), 12);
    lv_obj_set_style_bg_color(pm_bar, p->text_dim, 0);   /* trilho visível sobre o cartão */
    lv_obj_set_style_bg_opa(pm_bar, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(pm_bar, lv_color_hex(C_PERF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(pm_bar, 6, 0);
    lv_obj_set_style_radius(pm_bar, 6, LV_PART_INDICATOR);
    lv_bar_set_range(pm_bar, 0, 100);
    pm_shown = PM_OFF;
    pm_since = lv_tick_get();
    pm_update();
}

/* ---------------------------------------------------------------- ligar e desligar */

/* um motivo "forte": vídeo, TV, controle ou o botão (pausa tudo o que está marcado) */
static bool strong(void)
{
    return manual || (video && !suppressed && settings()->perf_auto) || (pad && !pad_suppressed && settings()->perf_pad) ||
           (tv && !tv_suppressed && settings()->perf_tv) || (stream && !stream_suppressed && settings()->perf_stream) ||
           (gps && !gps_suppressed && settings()->perf_gps);
}

/* o modo de uso pedido agora (TV e Big GPS nunca estão abertos juntos: uma tela por vez) */
static perf_mode_t mode_calc(void)
{
    if(tv && !tv_suppressed && settings()->perf_tv) return PERF_MODE_TV;
    if(gps && !gps_suppressed && settings()->perf_gps) return PERF_MODE_GPS;
    return PERF_MODE_NONE;
}

perf_mode_t perfui_mode(void) { return on ? mode_cur : PERF_MODE_NONE; }

static bool wanted(void) { return strong() || settings()->perf_always; }

/* o que pausar: só pelo plano de energia, GPS e tarefas (Bluetooth e telefone continuam: o
 * celular precisa conectar e as chamadas, chegar) */
static unsigned svc_now(void)
{
    /* plano de eficiência dos modos: vale por cima de "O que pausar" enquanto o app está aberto.
     * Bluetooth e telefone entram aqui, mas o que está conectado fica segurado (holds_update) */
    perf_mode_t m = mode_calc();
    if(m != PERF_MODE_NONE) return perfpolicy_pause(m);
    unsigned s = settings()->perf_svc;
    /* Big Remote Play: o controle Bluetooth precisa conectar e ficar (pode chegar depois de a
     * sessão começar) e as chamadas, chegar: nunca pausa Bluetooth nem telefone */
    if(stream && !stream_suppressed && settings()->perf_stream) s &= ~(PM_SVC_BT | PM_SVC_PHONE);
    return strong() ? s : s & (PM_SVC_GPS | PM_SVC_CRON);
}

static void apply(void)
{
    bool w = wanted();
    if(w == on) return;
    on = w;
    if(w) {
        if(settings()->perf_quiet || mode_calc() != PERF_MODE_NONE) fx_quiet(true);
        /* o Bluetooth de teclado/controle e o app aberto: perfmode_hold (tick) */
        perfmode_enter(svc_now(), false);
    }
    else {
        fx_quiet(false);
        perfmode_leave();
    }
    /* a janela que segura os toques só para o vídeo e o botão: com o controle a pessoa vai
     * jogar agora, e a TV já mostra "sintonizando"; a etiqueta avisa */
    if(manual || (video && !suppressed && settings()->perf_auto)) pm_open();
}

static void delay_done(lv_timer_t *t)
{
    (void)t;
    delay_t = NULL;
    apply();
}

/* troca agora ou depois de ms (o vídeo pode voltar/parar no meio: a espera recomeça) */
static void schedule(uint32_t ms)
{
    if(delay_t) { lv_timer_delete(delay_t); delay_t = NULL; }
    if(!ms) { apply(); return; }
    delay_t = lv_timer_create(delay_done, ms, NULL);
    lv_timer_set_repeat_count(delay_t, 1);
}

void perfui_video(bool playing)
{
    if(playing == video) return;
    video = playing;
    fx_keep_awake(playing);                      /* vídeo tocando: a tela não escurece */
    if(!playing) suppressed = false;             /* o próximo vídeo pode ligar de novo */
    if(manual) return;                           /* ligado à mão: o vídeo não muda nada */
    schedule((uint32_t)(playing ? settings()->perf_enter_s : settings()->perf_exit_s) * 1000u);
}

void perfui_video_gone(void)
{
    video = suppressed = false;
    fx_keep_awake(false);
    if(!manual) schedule(0);
}

void perfui_manual(bool m)
{
    manual = m;
    if(!m && video) suppressed = true;           /* desligou à mão: não volta neste vídeo */
    if(!m && pad) pad_suppressed = true;         /* nem com este controle */
    if(!m && tv) tv_suppressed = true;           /* nem com a TV aberta agora */
    if(!m && stream) stream_suppressed = true;
    if(!m && gps) gps_suppressed = true;
    schedule(0);
}

void perfui_disable(void)
{
    manual = false;
    if(video) suppressed = true;
    if(pad) pad_suppressed = true;
    if(tv) tv_suppressed = true;
    if(stream) stream_suppressed = true;
    if(gps) gps_suppressed = true;
    schedule(0);
}

static void mode_refresh(void);

void perfui_tv(bool open)
{
    if(open == tv) return;
    tv = open;
    if(!open) tv_suppressed = false;
    mode_refresh();
    if(manual) return;
    schedule(0);                                 /* abre: já; fecha: sai já (o app saiu) */
}

void perfui_stream(bool o)
{
    if(o == stream) return;
    stream = o;
    if(!o) stream_suppressed = false;
    if(manual) return;
    schedule(0);
}

void perfui_settings_changed(void)
{
    if(on) fx_quiet(settings()->perf_quiet || mode_calc() != PERF_MODE_NONE);   /* valem na hora */
    /* automático desligado no meio do vídeo: sai já; religado com o vídeo tocando: entra
     * depois da espera de sempre */
    if(manual || wanted() == on) return;
    schedule(on ? 0 : (uint32_t)settings()->perf_enter_s * 1000u);
}

perf_why_t perfui_why(void)
{
    if(perfmode_state() == PM_OFF && !on) return PERF_WHY_NONE;
    if(manual) return PERF_WHY_MANUAL;
    if(video && !suppressed && settings()->perf_auto) return PERF_WHY_VIDEO;
    if(tv && !tv_suppressed && settings()->perf_tv) return PERF_WHY_TV;
    if(stream && !stream_suppressed && settings()->perf_stream) return PERF_WHY_STREAM;
    if(gps && !gps_suppressed && settings()->perf_gps) return PERF_WHY_GPS;
    if(pad && !pad_suppressed && settings()->perf_pad) return PERF_WHY_PAD;
    return settings()->perf_always ? PERF_WHY_PLAN : PERF_WHY_VIDEO;
}

bool perfui_video_playing(void) { return video; }

/* ---------------------------------------------------------------- etiqueta flutuante */

static lv_obj_t *dlg_bd;                 /* fundo do diálogo da etiqueta (fecha se o modo desligar) */

static void dlg_gone(lv_event_t *e) { if(lv_event_get_target(e) == dlg_bd) dlg_bd = NULL; }

static void tag_answer(int b, void *ud)
{
    (void)ud;
    if(b == 0) perfui_disable();
    else if(b == 1) nav_go(SCR_PERFMODE, 0);
}

static void tag_click(lv_event_t *e)
{
    (void)e;
    perfui_dialog();
}

/* o X do canto: fecha como o Esc (o callback recebe -1) */
static void dlg_x_click(lv_event_t *e) { (void)e; dialog_key(false); }

void perfui_dialog(void)
{
    if(dlg_bd) return;
    char body[512];
    perf_why_t w = perfui_why();
    snprintf(body, sizeof body, "%s\n%s", tr(w == PERF_WHY_MANUAL ? S_PERFM_WHY_MANUAL : w == PERF_WHY_PAD ? S_PERFM_WHY_PAD
                                              : w == PERF_WHY_TV ? S_PERFM_WHY_TV : w == PERF_WHY_STREAM ? S_PERFM_WHY_STREAM : w == PERF_WHY_PLAN ? S_PERFM_WHY_PLAN
                                              : w == PERF_WHY_GPS ? S_PERFM_WHY_GPS
                                              : S_PERFM_WHY_VIDEO), tr(S_PERFM_TAG_BODY));
    static const char *b[2];
    b[0] = tr(S_PERFM_DISABLE);
    b[1] = tr(S_PERFM_CONFIGURE);
    lv_obj_t *card = dialog_open(LV_SYMBOL_CHARGE, lv_color_hex(C_PERF), tr(S_SET_PERFMODE), body, b, 2, 1, 0,
                                 tag_answer, NULL);
    dlg_bd = lv_obj_get_parent(card);
    lv_obj_add_event_cb(dlg_bd, dlg_gone, LV_EVENT_DELETE, NULL);
    /* fechar: um X no canto superior direito do cartão, fora do fluxo dos botões */
    lv_obj_t *x = bf_button(card, LV_SYMBOL_CLOSE, NULL, 0);
    lv_obj_add_flag(x, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(x, 48, 48);
    lv_obj_set_style_min_width(x, 0, 0);
    lv_obj_set_style_pad_all(x, 0, 0);
    lv_obj_set_style_radius(x, 24, 0);
    lv_obj_set_style_bg_opa(x, LV_OPA_TRANSP, 0);
    lv_obj_set_ext_click_area(x, 8);
    int32_t pad = lv_obj_get_style_pad_right(card, 0), padt = lv_obj_get_style_pad_top(card, 0);
    lv_obj_align(x, LV_ALIGN_TOP_RIGHT, pad - 8, -padt + 8);
    lv_obj_add_event_cb(x, dlg_x_click, LV_EVENT_CLICKED, NULL);
}

static void tag_deleted(lv_event_t *e)
{
    if(lv_event_get_target(e) == tag) tag = tag_lbl = NULL;    /* o tick refaz */
}

/* A etiqueta é uma marca d'água: sempre no mesmo lugar, no meio do topo, um pouco abaixo da
 * barra (y = TAG_Y, exista a barra ou não), quase invisível (TAG_OPA) e clicável: um toque a
 * acende e abre Desativar / Configurar (o X fecha). Quem desenha direto na tela (TV, Big Remote
 * Play, vídeo em tela cheia do Internet) a recebe em FBMARK_PATH e a mistura em cada quadro
 * (src/ui/core/fbmark.h): ela aparece igual por cima deles. */
#define TAG_H   30
#define TAG_Y   (BF_TOPBAR_H + 6)
#define TAG_OPA 77                       /* ~30 %: dá para achar, não atrapalha o que está por baixo */
#define BRIEF_MS 3000                    /* o aviso de entrada num modo, na própria etiqueta */
#define BRIEF_WAIT_MS 5000               /* o modo não ligou nesse prazo: o aviso não vale mais */
static bool mark_on;                     /* FBMARK_PATH gravado por este shell */
static lv_opa_t tag_opa = TAG_OPA;       /* a transparência de agora (o aviso a acende) */
static bool brief_want, brief_on;        /* aviso pedido (entrou num modo) / à mostra */
static uint32_t brief_req_at, brief_at;

static void tag_make(void)
{
    tag = lv_button_create(lv_layer_top());
    lv_obj_add_event_cb(tag, tag_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_remove_style_all(tag);
    lv_obj_set_size(tag, LV_SIZE_CONTENT, TAG_H);
    lv_obj_set_style_radius(tag, TAG_H / 2, 0);
    lv_obj_set_style_bg_color(tag, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(tag, LV_OPA_50, 0);
    lv_obj_set_style_border_width(tag, 1, 0);
    lv_obj_set_style_border_color(tag, lv_color_hex(C_PERF), 0);
    lv_obj_set_style_border_opa(tag, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(tag, 10, 0);
    lv_obj_set_style_pad_right(tag, 12, 0);
    lv_obj_set_style_pad_column(tag, 6, 0);
    lv_obj_set_flex_flow(tag, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tag, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_opa(tag, TAG_OPA, 0);               /* marca d'água */
    lv_obj_set_style_opa(tag, LV_OPA_COVER, LV_STATE_PRESSED);   /* o toque a acende */
    lv_obj_set_ext_click_area(tag, 10);
    lv_obj_add_flag(tag, LV_OBJ_FLAG_HIDDEN);
    bf_label(tag, LV_SYMBOL_CHARGE, BF_FONT_S, lv_color_hex(C_PERF));
    tag_lbl = bf_label(tag, tr(S_PERFM_TAG), BF_FONT_S, lv_color_white());
    lv_obj_align(tag, LV_ALIGN_TOP_MID, 0, TAG_Y);
    lv_obj_add_event_cb(tag, tag_click, LV_EVENT_CLICKED, NULL);
}

/* a etiqueta, já com a transparência, para quem desenha direto na tela (fbmark.h). Feita uma
 * vez por aparição (o desenho e o lugar não mudam); some com o modo */
static void mark_write(void)
{
#if LV_USE_SNAPSHOT
    lv_obj_update_layout(tag);
    lv_obj_set_style_opa(tag, LV_OPA_COVER, 0);          /* a cópia sai inteira; o alfa vai abaixo */
    lv_draw_buf_t *db = lv_snapshot_take(tag, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_style_opa(tag, tag_opa, 0);
    if(!db) return;
    lv_area_t c;
    lv_obj_get_coords(tag, &c);
    int w = db->header.w, h = db->header.h;
    /* a cópia inclui a área de toque estendida? o tamanho vem do objeto: alinha pelo centro */
    int x = c.x1 - (w - lv_area_get_width(&c)) / 2, y = c.y1 - (h - lv_area_get_height(&c)) / 2;
    if(w > 0 && h > 0 && w <= FBMARK_MAX_W && h <= FBMARK_MAX_H) {
        static uint16_t px[FBMARK_MAX_W * FBMARK_MAX_H];
        static uint8_t al[FBMARK_MAX_W * FBMARK_MAX_H];
        for(int j = 0; j < h; j++) {
            const uint8_t *row = db->data + (size_t)j * db->header.stride;
            for(int i = 0; i < w; i++) {
                const uint8_t *p = row + i * 4;              /* B G R A */
                px[j * w + i] = (uint16_t)((p[2] >> 3) << 11 | (p[1] >> 2) << 5 | p[0] >> 3);
                al[j * w + i] = (uint8_t)((p[3] * tag_opa * 32 + 255 * 255 / 2) / (255 * 255));
            }
        }
        FILE *f = fopen(FBMARK_PATH ".new", "wb");
        if(f) {
            uint16_t hd[4] = { (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h };
            size_t n = (size_t)w * h;
            bool ok = fwrite("BFM1", 1, 4, f) == 4 && fwrite(hd, sizeof hd[0], 4, f) == 4 &&
                      fwrite(px, sizeof px[0], n, f) == n && fwrite(al, 1, n, f) == n;
            if(fclose(f) == 0 && ok) mark_on = rename(FBMARK_PATH ".new", FBMARK_PATH) == 0;
            else unlink(FBMARK_PATH ".new");
        }
    }
    lv_draw_buf_destroy(db);
#endif
}

static void tag_anim_y(void *o, int32_t v) { lv_obj_set_style_translate_y(o, v, 0); }
static void tag_anim_opa(void *o, int32_t v) { lv_obj_set_style_opa(o, (lv_opa_t)v, 0); }

static void tag_show_now(void)
{
    lv_obj_remove_flag(tag, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(tag);
    lv_anim_delete(tag, NULL);
    lv_obj_set_style_translate_y(tag, 0, 0);
    mark_write();
    if(fx_anim() != LV_ANIM_ON) return;                  /* interface quieta: aparece na hora */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, tag);
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_values(&a, -6, 0);
    lv_anim_set_exec_cb(&a, tag_anim_y);
    lv_anim_start(&a);
    lv_anim_set_values(&a, LV_OPA_TRANSP, TAG_OPA);
    lv_anim_set_exec_cb(&a, tag_anim_opa);
    lv_anim_start(&a);
}

static void tag_hide_now(void)
{
    if(mark_on) { unlink(FBMARK_PATH); mark_on = false; }   /* some também de cima do vídeo */
    if(!tag || lv_obj_has_flag(tag, LV_OBJ_FLAG_HIDDEN)) return;
    lv_anim_delete(tag, NULL);
    if(brief_on) { brief_on = false; lv_label_set_text(tag_lbl, tr(S_PERFM_TAG)); }
    tag_opa = TAG_OPA;
    lv_obj_set_style_opa(tag, TAG_OPA, 0);
    lv_obj_add_flag(tag, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *perfui_tag_obj(void) { return tag; }

void perfui_svc_names(unsigned g, char *b, size_t n)
{
    static const struct { unsigned bit; int s; const char *fixed; } names[] = {
        { PM_SVC_GPS, -1, "GPS" }, { PM_SVC_BT, -1, "Bluetooth" },
        { PM_SVC_PHONE, S_PERFM_PHONE, NULL }, { PM_SVC_CRON, S_PERFM_TASKS, NULL },
    };
    b[0] = 0;
    for(size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        if(!(g & names[i].bit)) continue;
        size_t k = strlen(b);
        snprintf(b + k, n - k, "%s%s", k ? ", " : "", names[i].fixed ? names[i].fixed : tr(names[i].s));
    }
}

/* ---------------------------------------------------------------- apps e controle */

#define HOLD_RELEASE_MS 3000             /* saiu do app: pausa de novo depois disso */
#define PAD_ENTER_MS    1000             /* controle conectou: liga depois disso */

/* o que o app aberto precisa que esteja rodando */
static unsigned app_needs(bf_screen_t s)
{
    switch(s) {
    /* Big GPS: com o modo GPS (perf_gps), o GPS fica e Bluetooth e telefone seguem a regra "só
     * pausa sem nada conectado" (holds_update); sem ele, nada essencial pausa
     * (docs/big-gps-2/25-modo-performance.md) */
    case SCR_MAP: return settings()->perf_gps ? PM_SVC_GPS : PM_SVC_GPS | PM_SVC_BT | PM_SVC_PHONE;
    case SCR_GPS: return PM_SVC_GPS;
    case SCR_PHONE: return PM_SVC_PHONE | PM_SVC_BT;
    case SCR_BLUETOOTH: return PM_SVC_BT;
    case SCR_BRP: return PM_SVC_BT;                  /* o controle Bluetooth do jogo */
    default: return 0;
    }
}

static unsigned hold_now;
static bool hold_dropping;               /* o app largou algo: pausa de novo em hold_drop_at */
static uint32_t hold_drop_at;
static bool prio_now, bt_boosted;
static uint32_t boost_at, tv_boost_at;

/* Conexões reais, lidas a cada 1 s (arquivos em /run, sem D-Bus): algum aparelho Bluetooth
 * conectado (celular, fone, teclado, controle) e o celular em especial (o canal de chamadas
 * do bfphone, ou um aparelho conectado com ícone de telefone, que é o que o BlueZ informa). */
static bool conn_bt, conn_phone;

static void conn_update(bool fresh)
{
    static uint32_t at;
    if(!fresh && at && lv_tick_elaps(at) < 1000) return;
    at = lv_tick_get();
    static bt_info_t b;
    static ph_info_t ph;
    bt_read(&b);
    conn_bt = conn_phone = false;
    for(int i = 0; i < b.ndev; i++) {
        if(!b.dev[i].connected) continue;
        conn_bt = true;
        if(!strncmp(b.dev[i].icon, "phone", 5)) conn_phone = true;
    }
    ph_read(&ph);
    if(ph.available && ph.link) conn_phone = conn_bt = true;
}

#define MODE_GRACE_MS 15000              /* começo de um modo: o celular ainda pode conectar */

/* o que fica: o app aberto, o Bluetooth com algo conectado, telefone e Bluetooth com o celular
 * e, nos primeiros 15 s de um modo, o que ainda roda (perfpolicy.c, testado) */
static unsigned holds_need(void)
{
    return perfpolicy_holds(mode_calc(), app_needs(nav_current().scr), hwkbd_bt_connected() || conn_bt,
                            conn_phone, lv_tick_elaps(mode_since) < MODE_GRACE_MS, perfmode_paused());
}

static void holds_update(void)
{
    conn_update(false);
    unsigned need = holds_need();
    unsigned add = need & ~hold_now, drop = hold_now & ~need;
    if(add) {                                    /* o app está esperando: já */
        hold_now |= add;
        perfmode_hold(hold_now);
    }
    if(!drop) { hold_dropping = false; return; }
    if(!hold_dropping) {                         /* pode voltar ao app logo: espera um pouco */
        hold_dropping = true;
        hold_drop_at = lv_tick_get() + HOLD_RELEASE_MS;
    } else if((int32_t)(lv_tick_get() - hold_drop_at) >= 0) {
        hold_dropping = false;
        conn_update(true);                       /* soltar com o estado de agora: um celular que */
        hold_now = holds_need();                 /* acabou de conectar não perde o telefone */
        perfmode_hold(hold_now);
    }
}

/* prioridade: a interface com o modo ligado (perf_prio) ou com um controle; o bluetoothd com um
 * controle (ele lê o canal HID do Bluetooth e entrega ao kernel pelo uhid); no modo TV o bftvd
 * fica protegido do OOM e com nice -10 (ele já se põe assim; de novo a cada 5 s porque trocar
 * de canal é um processo novo). No modo TV a interface continua em -5, abaixo do player: medido
 * na Globo sem o PC, com ela em 0 o toque esperava até 618 ms (p95 190 ms); em -5, até 141 ms
 * (p95 70 ms), com os mesmos quadros (13-15 qps). Ociosa, a prioridade não custa nada. */
static void prio_update(void)
{
    bool want = (on && settings()->perf_prio) || pad;
    if(want != prio_now && setpriority(PRIO_PROCESS, 0, want ? -5 : 0) == 0) {
        prio_now = want;
        FILE *f = fopen("/proc/self/oom_score_adj", "w");   /* a interface nunca é morta por falta de memória */
        if(f) { fprintf(f, "%d\n", want ? -1000 : 0); fclose(f); }
    }
    /* de novo a cada 5 s: o bluetoothd pode ter reiniciado */
    if(pad && (!bt_boosted || (int32_t)(lv_tick_get() - boost_at) >= 5000)) {
        bt_boosted = perfmode_renice("bluetoothd", -10);
        boost_at = lv_tick_get();
    } else if(!pad && bt_boosted) {
        perfmode_renice("bluetoothd", 0);
        bt_boosted = false;
    }
    /* no máximo a cada 5 s, achando o bftvd ou não: na lista de canais (vídeo parado) não há
     * player, e tentar a cada tique varria o /proc inteiro 5 vezes por segundo */
    bool tv_mode = on && mode_cur == PERF_MODE_TV;
    if(tv_mode && (!tv_boost_at || (int32_t)(lv_tick_get() - tv_boost_at) >= 5000)) {
        perfmode_renice("bftvd", -10);
        tv_boost_at = lv_tick_get() | 1;             /* 0 = ainda não tentou neste modo */
    } else if(!tv_mode) tv_boost_at = 0;
}

/* /run/bigfoston-quiet: existe enquanto um modo vale; as tarefas periódicas do sistema
 * (o relatório "alive" do S10bigfoston) pulam a vez. /run some ao desligar. */
#define QUIET_FLAG "/run/bigfoston-quiet"
static void quiet_flag(bool want)
{
    static int has = -1;
    if(has == (int)want) return;
    if(want) { int fd = open(QUIET_FLAG, O_WRONLY | O_CREAT | O_CLOEXEC, 0644); if(fd >= 0) close(fd); }
    else unlink(QUIET_FLAG);
    has = want;
}

/* o modo mudou agora: marca o começo (carência) e acerta o que fica segurado ANTES de o
 * perfmode receber o alvo novo (senão a carência chegaria um tique depois do stop) */
static void mode_refresh(void)
{
    perf_mode_t m = mode_calc();
    if(m != mode_cur) {
        mode_cur = m;
        mode_since = lv_tick_get();
        brief_want = m != PERF_MODE_NONE;        /* entrou num modo: a etiqueta avisa (brief_start) */
        brief_req_at = mode_since;
    }
    holds_update();
}

/* modo de uso: carência, sinal para o sistema e, ao entrar no GPS, a TV desligada de vez */
static void mode_update(void)
{
    bool g = nav_current().scr == SCR_MAP;
    if(g != gps) {
        gps = g;
        if(!g) gps_suppressed = false;           /* a próxima abertura pode ligar de novo */
        if(g && settings()->perf_gps) scr_tv_ensure_off();   /* nada de TV órfã no modo GPS */
        mode_refresh();
        if(!manual) schedule(0);
    }
    mode_refresh();
    pm_state_t ps = perfmode_state();
    quiet_flag(mode_cur != PERF_MODE_NONE && (ps == PM_ON || ps == PM_ENTERING));
}

static void pad_update(void)
{
    bool now = gamepad_count() > 0;
    if(now == pad) return;
    pad = now;
    if(!pad) pad_suppressed = false;             /* o próximo controle pode ligar de novo */
    if(manual || (video && !suppressed && settings()->perf_auto)) return;   /* outro motivo segura */
    /* o controle saiu: desliga já (esperar perf_exit_s só pausava e religava serviços à toa) */
    schedule(pad ? PAD_ENTER_MS : 0);
}

/* Aviso de entrada num modo, na própria etiqueta: ela acende por BRIEF_MS com o que pausou
 * ("Modo TV · Pausado: GPS, tarefas") e volta à marca d'água. Na etiqueta, e não num aviso à
 * parte, porque ela já aparece por cima do vídeo e não conta como algo aberto por cima da TV
 * (um aviso comum pausaria o desenho da TV justamente na entrada). */
static void brief_start(void)
{
    char names[96], list[128], text[160];
    perfui_svc_names(svc_now() & ~hold_now, names, sizeof names);
    if(names[0]) snprintf(list, sizeof list, tr(S_PERFM_PAUSED_LIST), names);
    else snprintf(list, sizeof list, "%s", tr(S_PERFM_NONE_PAUSED));
    int fmt = mode_cur == PERF_MODE_GPS ? S_PERFM_BRIEF_GPS : S_PERFM_BRIEF_TV;
    snprintf(text, sizeof text, tr(fmt), list);
    /* a etiqueta inteira precisa caber na cópia para o vídeo (FBMARK_MAX_W, com o ícone e as
     * margens): longa demais, sai o "Pausado:" */
    lv_point_t sz;
    const lv_font_t *f = lv_obj_get_style_text_font(tag_lbl, 0);
    lv_text_get_size(&sz, text, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if(sz.x > FBMARK_MAX_W - 80 && names[0]) snprintf(text, sizeof text, tr(fmt), names);
    lv_anim_delete(tag, NULL);
    lv_obj_set_style_translate_y(tag, 0, 0);
    lv_label_set_text(tag_lbl, text);
    tag_opa = LV_OPA_COVER;
    lv_obj_set_style_opa(tag, tag_opa, 0);
    brief_on = true;
    brief_at = lv_tick_get();
    mark_write();
}

static void brief_end(void)
{
    brief_on = false;
    lv_label_set_text(tag_lbl, tr(S_PERFM_TAG));
    tag_opa = TAG_OPA;
    lv_obj_set_style_opa(tag, tag_opa, 0);
    mark_write();
}

static void tick(lv_timer_t *t)
{
    (void)t;
    pad_update();
    mode_update();                                 /* acerta também o que fica segurado */
    prio_update();
    /* interface: sem sons de toque e animações (perf_quiet, e sempre nos modos TV e GPS) */
    fx_quiet(on && (settings()->perf_quiet || mode_cur != PERF_MODE_NONE));
    /* fundo liso enquanto ligado: o papel de parede sai do que é redesenhado */
    static bool plain;
    pm_state_t ps = perfmode_state();
    bool want_plain = settings()->fx_level == BF_FX_MIN ||
                      ((settings()->perf_wall || mode_cur != PERF_MODE_NONE) && strong() && (ps == PM_ON || ps == PM_ENTERING));
    if(on) perfmode_enter(svc_now(), false);       /* o motivo mudou (o vídeo começou com o plano) */
    if(want_plain != plain) { plain = want_plain; app_wallpaper_plain(plain); }
    pm_update();
    if(!tag) tag_make();
    pm_state_t st = perfmode_state();
    if(dlg_bd && (st == PM_OFF || st == PM_LEAVING)) {   /* o modo desligou: o diálogo não vale mais */
        lv_obj_t *d = dlg_bd;
        dlg_bd = NULL;
        lv_obj_delete_async(d);
    }
    /* a etiqueta: à mostra enquanto o modo estiver ligado (ou ligando), por qualquer motivo e
     * em qualquer tela, sempre no mesmo lugar (tag_make) */
    bool show = settings()->perf_tag && (st == PM_ON || st == PM_ENTERING);
    if(show && lv_obj_has_flag(tag, LV_OBJ_FLAG_HIDDEN)) tag_show_now();
    else if(show && lv_obj_get_index(tag) != (int32_t)lv_obj_get_child_count(lv_layer_top()) - 1 && !pm_box && !dlg_bd &&
            !dialog_is_open())
        lv_obj_move_foreground(tag);                    /* algo novo na camada (tela cheia): por cima de novo */
    else if(!show) tag_hide_now();
    /* o aviso de entrada num modo (brief_start) */
    if(brief_want && (!settings()->perf_tag || lv_tick_elaps(brief_req_at) > BRIEF_WAIT_MS)) brief_want = false;
    if(show && brief_want && mode_cur != PERF_MODE_NONE) { brief_want = false; brief_start(); }
    else if(brief_on && lv_tick_elaps(brief_at) > BRIEF_MS) brief_end();
}

void perfui_init(void)
{
    unlink(QUIET_FLAG);                          /* sobras de um shell anterior */
    unlink(FBMARK_PATH);
    if(!tick_t) tick_t = lv_timer_create(tick, 200, NULL);
    if(perfmode_recover()) pm_open();            /* sobrou de um shell anterior: religa */
}
