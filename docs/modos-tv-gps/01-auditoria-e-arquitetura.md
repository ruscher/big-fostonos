# Etapa 1 — Auditoria e arquitetura

Levantamento feito no código (branch `main`, commit `095ec51`) e no aparelho ligado pela rede
USB (192.168.7.2), antes de qualquer mudança.

## O que o sistema é de verdade

O pedido citava WindowMaker, KDE Plasma, GTK4/libadwaita, QML, systemd e cpufreq. Nada disso
existe no FOSTON; a auditoria partiu do que existe:

| Camada | No aparelho | Consequência para o pedido |
|---|---|---|
| CPU | ARM1176JZ-S, 468 MHz **fixos**, sem FPU, sem `cpufreq` | não há governor nem EPP para ajustar; o "plano" age no que roda |
| Memória | 128 MB, sem swap; zram no kernel, **desligado de propósito** ([../performance/05-zram.md](../performance/05-zram.md)) | swappiness não se aplica; zram foi medido e descartado (caro neste ARM) |
| Sistema | Buildroot, BusyBox init (`/etc/init.d/S*`), sem cgroups, sem `ionice` | prioridade só por `nice`/`oom_score_adj`; serviços por script |
| Interface | `bigfoston-shell`: C11 + LVGL 9.2 no `/dev/fb0` (800×480 RGB565), toque resistivo | a etiqueta é um objeto LVGL; não há gerenciador de janelas |
| TV | `bftvd` (cartão SD) desenha direto no framebuffer; `bftv-relay` no PC decodifica pelo USB | nada do LVGL pode ficar por cima da área do vídeo |
| GPS | `bfgps` roda o `gps.dll` do WinCE; Big GPS é o mapa dentro do shell | o GPS é o maior consumidor parado (9,9 % da CPU) |

No PC (o computador de desenvolvimento) roda o serviço `bigfoston-tv-relay` (systemd do
usuário) e o instalador GTK4/libadwaita (`installer/`), que não fazem parte deste fluxo.

## Inventário

907 arquivos versionados. O que interessa a esta etapa:

| Pasta | Arquivos | Linhas | Conteúdo |
|---|---|---|---|
| `src/ui/core` | 107 | 18 666 | estado, serviços, energia, modo performance, ajustes, i18n |
| `src/ui/screens` | 39 | 21 361 | uma tela por arquivo (`scr_tv.c`, `scr_map.c` com 4 030 linhas) |
| `src/ui/widgets` | 28 | 3 183 | barra de cima, diálogos, páginas |
| `src/ui/biggps` | 43 | 6 369 | motor do mapa, rotas, voz, alertas |
| `src/tv` | 9 | 2 653 | `bftvd` (player), `siano` (sintonizador), `bftv-relay` (PC), `yuvz` |
| `src/bt` | 12 | 4 912 | `bfbt-agent`, `bfphone` |
| `firmware/buildroot` | 55 | — | rootfs, `init.d`, configuração do kernel |

Serviços em `/etc/init.d` que o modo controla: `S60gps` (`bfgps`), `S50crond`, `S43bfphone`,
`S42bfbt-agent`, `S41bluealsa`, `S40bluetoothd`, `S35bluetooth` (`bfbt-attach`).

## Fluxos antes desta etapa

```
Plano de energia (Ajustes > Potência) ──grava──► settings.conf (brilho, sons, efeitos, perf_always…)
Efeitos visuais (Ajustes > Desempenho) ──grava──► fx_level, anim
                     │
Motivos ────────────►│ perfui (tique de 200 ms)
 vídeo, controle,    │   wanted()/strong() ──► perfmode_enter(grupos) ──thread──► /etc/init.d/* stop|start
 TV, Big Remote Play,│   holds (app aberto, Bluetooth conectado) ──► perfmode_hold()
 manual, plano       │   fx_quiet, papel de parede liso, nice -5 da interface
                     └── etiqueta "Performance" (8, 54) e janela de progresso
```

- **TV:** abrir a TV ligava o modo com `perf_svc` (padrão: GPS, Bluetooth, telefone, tarefas).
  O telefone pausava **mesmo com o celular conectado** (só o Bluetooth tinha a regra "fica se
  conectado").
- **Big GPS:** o mapa segurava GPS, Bluetooth e telefone; sem outro motivo, o modo nem ligava.
  Não havia "modo GPS".
- **Etiqueta:** escondida na TV, no Big Remote Play, no controle, no plano de energia, e some
  depois de 6 s com controle/TV/stream.
- **Big GPS, destino:** quatro botões (Carro aceso sempre), nenhuma memória, nenhum
  automático; o painel de rotas com "Iniciar" sem automático.

## Defeitos e gargalos achados

| # | Onde | Defeito | Evidência | Gravidade |
|---|---|---|---|---|
| 1 | `bftv-relay.c` (PC) | sem keepalive nem prazo: com o FOSTON reiniciado ou o cabo arrancado, a leitura espera **para sempre** numa conexão meio-aberta e o relay nunca reconecta | `ss` no PC: `ESTAB` com 192.168.7.2:48122 há 9 h, 51 ms de CPU; FOSTON com `relay=0` e o PC ao lado | **crítica**: a TV decodifica no ARM (4 qps no SBT) quando devia usar o PC |
| 2 | `scr_tv.c` `destroy` | `waitpid` bloqueante no `bftvd` (até 2 s) e no `bftvd -O` (sem prazo) | sair da TV: 1026 / 1082 ms de laço parado | alta |
| 3 | `scr_tv.c` `on_scan` | espera o vídeo parar com `usleep` no laço (até 2 s) | código | média |
| 4 | `scr_tv.c` `update_ui` | reaplicava estilo dos 3 botões de fonte e do ponto de gravar a cada 500 ms (cada set invalida) | `perfstat trace`: as mesmas áreas 2×/s | média |
| 5 | `topbar.c` `icon_state` | cor e opacidade de cada ícone reaplicadas a cada segundo, em **todas** as telas | `perfstat trace`: cada ícone 4×/s | média |
| 6 | LVGL 9.2 `lv_obj_remove_flag` | `HIDDEN` removido de objeto já visível invalida e suja o layout do pai | código do LVGL; a barra faz isso a cada segundo | média |
| 7 | `bftvd.c` | sem cabo USB, espera 1,5 s pelo PC a cada abertura/troca de canal | código; `/sys/class/udc/*/state` diz se há PC | baixa |
| 8 | `perfui.c` | telefone pausado com o celular conectado (as chamadas não chegam) | código, regra só para o Bluetooth | alta |
| 9 | `perfui.c` | etiqueta escondida justamente na TV e onde o modo mais vale | código (`brief`, exclusões) | requisito |
| 10 | `scr_tv.c` | um `bftvd` travado (SPI, `/dev/dsp`) deixa a TV parada sem saída | sem vigia | média |
| 11 | `S10bigfoston` | relatório "alive" a cada 10 s: ~6 processos, **1,5 % da CPU** o tempo todo | 90 ticks em 60 s no aparelho | baixa |

Achados que **não** eram defeitos (conferidos):
- `perfmode` já tinha recuperação: a nota `/run/bigfoston-perfmode` religa o que um shell
  anterior parou.
- O `bftvd` já tinha a escada de decodificação, áudio em thread própria e volta ao local
  quando o PC sai.
- O zram está desligado por decisão medida (não é esquecimento).
- A falta de `cpufreq` é do SoC (MT3351), não de configuração.

## Arquitetura depois

```
                 ┌──────────── perfpolicy.c (regras puras, testadas) ────────────┐
                 │  pause(modo)          holds(modo, app, bt, celular, carência)  │
                 └────────────────────────────▲───────────────────────────────────┘
Motivos ───► perfui (tique 200 ms)             │
 vídeo, controle, stream, manual, plano        │
 TV aberta  ──perfui_tv──► modo TV  ──┐        │
 mapa aberto ─(nav)──────► modo GPS ──┴► mode_refresh ─► holds ─► perfmode_hold
                                       ├► svc_now ─► perfmode_enter ─► thread ─► init.d
                                       ├► interface: fx_quiet, papel liso, prioridades, OOM
                                       ├► /run/bigfoston-quiet ─► S10bigfoston pula a vez
                                       └► etiqueta: marca d'água fixa (topo central) ─► /run/bigfoston-mark
                                                    ─► bftvd, bfrp, webremote a misturam em cada quadro
TV: scr_tv ─► bftvd (fork) ─► encerrando ─► desligando (-O) ─► livre   (sem esperar no laço)
            ◄── relay=1/0 ── /run/bigfoston-tv ◄── bftvd ◄── PC (bftv-relay, keepalive)
GPS: destino ─► AG_VEH (10 s) ─► rota ─► AG_GO (10 s) ─► navegação
```

As etapas 2 a 5 detalham cada parte; a 6 tem a validação.
