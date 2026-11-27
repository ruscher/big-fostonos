# Modos TV e GPS, TV sem travar e GPS que segue sozinho

Estado em 27/11/2026, medido no FOSTON FS-707DC (ARM1176 a 468 MHz fixos, 128 MB), BigFoston OS
0.6.5 com o shell e o `bftvd` desta etapa.

O pedido tinha quatro partes:
1. uma máquina de estados única para "Modo performance", "Desempenho", "Plano de energia" e um
   novo "Plano de eficiência", com regras próprias quando a TV ou o GPS estão em uso;
2. a etiqueta "Modo performance" sempre visível enquanto o modo vale (revisado: marca d'água fixa
   no topo central, um pouco abaixo da barra, clicável);
3. a TV sem travar, principalmente sem o cabo USB;
4. no Big GPS, lembrar o transporte e escolher/iniciar sozinho em 10 s.

| Etapa | Documento | Estado |
|---|---|---|
| 1 | [Auditoria e arquitetura](01-auditoria-e-arquitetura.md) | concluída: inventário, fluxos, 11 defeitos achados |
| 2 | [Modos, energia e eficiência](02-sistema-performance-energia-eficiencia.md) | **FUNCIONANDO NO APARELHO** |
| 3 | [TV off-line e fluidez](03-tv-offline-decoder-e-fluidez.md) | **FUNCIONANDO NO APARELHO** |
| 4 | [GPS: transporte e contagens de 10 s](04-gps-ux-automacao-10s.md) | **FUNCIONANDO NO APARELHO** |
| 5 | [Etiqueta flutuante e polimento](05-etiqueta-flutuante-e-polimento-uxui.md) | **FUNCIONANDO NO APARELHO** |
| 6 | [QA e checklist final](06-qa-validacao-e-checklist-final.md) | 0 avisos de compilação, analisador e shellcheck; testes passando |

## Em números (antes → depois, medido no aparelho)

| Medida | Antes | Depois |
|---|---|---|
| Relay da TV depois de o FOSTON reiniciar ou o cabo sair | preso para sempre (9 h em conexão morta; a TV decodificava no FOSTON com o PC ao lado) | reconecta sozinho em ~9 s |
| Interface redesenhando durante a TV | 60 quadros em 20 s (3,0/s) | **7–8 em 20 s** (−88 %) |
| Laço da interface durante a TV (p95) | 21–24 ms | **2 ms** |
| Tela parada ao sair da TV (pior volta do laço) | 1026 / 1082 ms | **476 / 564 ms** |
| Tela parada ao começar uma busca de canais | até 2 s (espera no laço) | 77 ms |
| Abrir/trocar canal sem o cabo USB | espera 1,5 s pelo PC a cada vez | 0 s (só espera se há PC no cabo) |
| TV sem o PC (Globo) | 11–13 qps (doc anterior) | 13–15 qps, 0 cortes de som, 0 TS perdido |
| Relatório periódico do sistema (`S10bigfoston`) nos modos | 1,5 % da CPU | 0 (pula a vez) |
| Etiqueta do modo | escondida na TV, no plano, no controle; some em 6 s | sempre, no mesmo lugar, inclusive sobre o vídeo em tela cheia |
| Diálogo por cima da TV | o quadro seguinte o apagava | o player pausa o desenho: 0 quadros com o diálogo, som contínuo |

## Arquivos

| Arquivo | Papel |
|---|---|
| `src/ui/core/perfpolicy.c/.h` | as regras dos modos TV e GPS (puras, testadas em `tests/test_perfpolicy.c`) |
| `src/ui/core/perfui.c/.h` | máquina de estados do modo: motivos, modos, carência, interface, etiqueta |
| `src/ui/core/perfmode.c` | para e religa os serviços (`/etc/init.d`), nota em `/run` para recuperação |
| `src/ui/widgets/topbar.c` | ícones sem redesenho à toa |
| `src/ui/core/fbmark.h` | a etiqueta por cima de quem desenha direto na tela (TV, Big Remote Play, vídeo do Internet) e a pausa do desenho da TV com um diálogo aberto |
| `src/remoteplay/video.c`, `src/ui/core/webremote.c` | misturam a etiqueta em cada quadro |
| `src/ui/screens/scr_tv.c` | ciclo do `bftvd` sem esperar na interface, watchdog, decodificação no rodapé |
| `src/ui/screens/scr_map.c` | transporte lembrado, contagens de 10 s |
| `src/ui/screens/scr_perfmode.c` | Ajustes > Modo performance: modos TV e GPS e resumo ao vivo |
| `src/ui/core/ctl.c` | `tap X Y` e `mapdemo N` para testar no aparelho por SSH |
| `src/ui/patches/0006-…` | LVGL: mostrar/esconder sem efeito quando nada muda |
| `src/tv/bftv-relay.c` | keepalive: não fica preso numa conexão morta |
| `src/tv/bftvd.c` | não espera o PC sem cabo USB |
| `firmware/…/init.d/S10bigfoston` | relatório periódico pula a vez nos modos |
