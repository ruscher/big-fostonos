# Etapa 6 — Validação e checklist final

Tudo aqui foi rodado em 27/11/2026, na branch `feat/modos-tv-gps`. Os testes funcionais foram no
FOSTON FS-707DC ligado pelo cabo USB, com o shell e o `bftvd` desta etapa e o celular POCO X4
conectado por Bluetooth.

## Compilação e análise estática

O pedido citava `ruff`/`flake8`/`mypy`, `qmllint` e `eslint`. Esta etapa não tem Python, QML nem
JavaScript novos; os equivalentes para C e shell são os abaixo.

| Verificação | Comando | Resultado |
|---|---|---|
| Simulador, do zero (`rm -rf build/sim`) | `make sim` (`-Wall -Wextra`) | **0 avisos, 0 erros** |
| Caminho do alvo no gcc do PC, do zero | `make target-hostcheck` | **0 / 0** |
| Alvo ARM, do zero | `make VARIANT=target CROSS_COMPILE=arm-buildroot-linux-musleabi-` (GCC 14.3) | **0 / 0**, 1 935 996 bytes |
| Player da TV | `BIGFOSTON_BUILD=… tools/bigfoston/build-tv.sh` (`-Wall -Wextra`) | **0 / 0** |
| Relay do PC | `cc -Wall -Wextra … bftv-relay.c` | **0 / 0** |
| Motor do Big Remote Play | `BIGFOSTON_BUILD=… tools/bigfoston/build-remoteplay.sh` | **0 / 0** |
| Revisão do diff | leitura dos arquivos centrais | 3 achados corrigidos: reforço de prioridade varrendo o `/proc` a cada 200 ms sem player (agora a cada 5 s); `-O` podendo correr junto com um órfão saindo; variável sem uso |
| Analisador estático do GCC (`-fanalyzer`) | `perfui`, `perfpolicy`, `ctl`, `settings`, `scr_tv`, `scr_perfmode`, `topbar`, `scr_map`, `scr_web`, `scr_brp`, `webremote`, `bftv-relay` (PC) e `bftvd` (ARM) | **0** (havia 1 em `route_request`, código anterior: falso positivo de limite, reforçado com cópia local limitada) |
| Shell | `shellcheck` 0.11 em `S10bigfoston`, `build-tv.sh`, `build-remoteplay.sh`, `tv-relay-install.sh` | **0 avisos** (4 anteriores no `build-remoteplay.sh`: divisão intencional, agora citada na exceção) |
| Patch do LVGL | `patch --dry-run` no `lv_obj.c` original | aplica limpo |
| Revisão automática do PR (CodeRabbit, 10 comentários) | conferidos no código | **10 corrigidos**: busca da TV que não começa (volta aos canais); busca esperando um player travado (agora pelo encerramento com prazo e `SIGKILL`); vários órfãos vigiados ao mesmo tempo; contagem do Iniciar parando se o GPS cai; rota atrasada reabrindo o painel depois de Encerrar; telefone solto com o estado de conexão de 1 s antes; etiqueta escurecendo nas linhas puladas do `bfrp` (e presa no vídeo do Internet ao sumir); milissegundos em 64 bits (estouro com 24,9 dias ligado); `BIGFOSTON_BUILD` relativo; instalador do relay na mesma árvore |
| `bfrp` e `bftvd` depois das correções | toolchains refeitos (`out-cxx` de `bigfoston_cxx_defconfig`, `out-web`), fontes do Buildroot conferidas pelo hash, Moonlight fixado no commit `33142e5` | **0 avisos** no código do `bftvd`, `bftv` e `bfrp` (os 13 avisos do log são do próprio FFmpeg); instalados no cartão (os anteriores em `*.27nov`). TV medida com o `bftvd` novo: 30 qps pelo PC, 0 quadros com o diálogo aberto, 92 em 3 s depois do ✕. O `bfrp` não foi exercitado (precisa de uma sessão do Sunshine) |

## Testes automatizados

| Suíte | Resultado |
|---|---|
| `make test` (19 programas com ASan/UBSan, inclui os novos `test_perfpolicy` e `test_fbmark`) | **todos passaram** (código 0), 0 avisos de sanitizador |
| `test_perfpolicy`: TV/GPS × Bluetooth/celular conectado × carência × o que já está pausado (exaustivo: nunca pausa o que está conectado; o modo GPS nunca pausa o GPS) | **ok** |
| `test_fbmark`: mistura RGB565, recorte na região desenhada, aviso de marca que aparece/muda/some com a área a refazer, arquivo inválido | **ok** |
| `pytest tests` (instalador e protocolo do agente, como no CI) | **9 passaram** |
| `make -C src/wince/agent host-test` (CI) | **ok** |
| Capturas do simulador (`--screenshot`, 174 telas) contra as de antes das mudanças | só mudaram relógio/data/uptime/CPU, a página do Modo performance (nova seção) e a etiqueta (visual novo e presente na TV): conferido lado a lado |

## Testes no aparelho

| # | Cenário | Esperado | Resultado |
|---|---|---|---|
| 1 | FOSTON reiniciado com o relay do PC ativo | o relay reconecta | **antes: preso 9 h**; depois: rede USB derrubada 15 s, PC descartou a sessão e reconectou; `relay=1`, 24 qps, 0 cortes |
| 2 | Abrir a TV com o celular conectado | GPS e tarefas pausam; Bluetooth e telefone ficam | `bfgps`, `crond` parados; `bluetoothd`, `bluealsa`, `bfbt-agent`, `bfphone` com os mesmos PIDs depois dos 15 s de carência |
| 3 | Prioridades no modo TV | `bftvd` nice −10, OOM −1000 | conferido em `/proc/<pid>/stat` e `oom_score_adj` |
| 4 | TV sem o PC (Globo) | sem travar | 13–15 qps, 0 cortes de som, 0 TS perdido, 0 drops; rodapé "Decodificado no FOSTON" |
| 5 | Liga e desliga o PC com a TV tocando | troca sem parar | PC→FOSTON→PC: 0 cortes, 0 drops em todas |
| 6 | Sair da TV | sem congelar | pior volta 476 / 564 ms (antes 1026 / 1082 ms); `bftvd` saiu; sintonizador `parado` |
| 7 | 6 trocas de canal em 4,2 s | sem órfão | nunca mais de 1 `bftvd`; pior volta 224 ms; canal final salvo |
| 8 | Buscar canais e cancelar | sem travar, lista mantida | começar: 77 ms; cancelar: lista idêntica (`cmp`), sem `.tmp`, 0 `bftvd` |
| 9 | Tela cheia da TV com o modo ligado | fica em tela cheia | ficou; ao sair, barra e etiqueta voltam |
| 10 | Abrir o Big GPS | modo GPS: tarefas pausam, GPS fica, Bluetooth/telefone ficam (celular conectado) | conferido; `/run/bigfoston-quiet` criado |
| 11 | Sair do Big GPS e da TV | tudo volta | nota e sinalizador apagados; `bfgps` e `crond` de volta |
| 12 | 6 ciclos TV ↔ Big GPS | estável | mesmo processo do shell; RSS 7472 → 7488 KB; 0 órfãos; 0 zumbis (`/proc/*/stat`) |
| 13 | Contagens do Big GPS | etapa 4 | todas as linhas da tabela da etapa 4 |
| 14 | Etiqueta (revisão: marca d'água fixa no topo central) | sempre no mesmo lugar, clicável | Início, Ajustes, mapa (sem barra), navegando, TV e TV em tela cheia (misturada pelo `bftvd`): capturas na etapa 5 |
| 14b | Tocar na etiqueta sobre a TV | diálogo inteiro, vídeo não o apaga | `bigfoston-tv-hold` criado; 0 quadros desenhados em 3 s com o diálogo (67 antes); som sem cortes; ✕ fecha e o vídeo volta (94 em 3 s) |
| 14c | Tocar na etiqueta na TV em tela cheia | abre o diálogo | saiu da tela cheia, diálogo visível, vídeo pausado |
| 14d | Desligar o modo | a etiqueta some de tudo | `/run/bigfoston-mark` e `bigfoston-tv-hold` removidos |
| 14e | Big Remote Play e vídeo em tela cheia do Internet | a etiqueta misturada na imagem | **compilado e instalado (`bfrp` idêntico ao build), não exercitado**: precisa de uma sessão de jogo (Sunshine) ou do navegador pelo PC |
| 15 | Redesenho na TV, A/B com o binário antigo | menos trabalho | 60 → 7–8 quadros em 20 s; laço p95 24 → 2 ms |
| 17 | `bftvd` órfão (iniciado pelo SSH, fora do shell, tocando) e abrir o Big GPS | modo GPS desliga a TV | encerrado na abertura do mapa ("tv: player órfão … encerrado"); o `-O` só depois de ele sumir |
| 18 | Aviso ao entrar num modo (TV, depois TV → mapa) | a etiqueta acende 3 s com o que pausou | "Modo TV · Pausado: GPS, tarefas" e "Modo GPS · Pausado: tarefas" (celular conectado), de volta à marca d'água; a TV não pausou o desenho |
| 19 | Vigia do shell: `hang 60` no canal de controle | o shell é reiniciado | saiu aos 50 s com o código 70 e o motivo no log; voltou sozinho. Pior volta do laço em uso normal: 537 ms |
| 16 | Logs depois de toda a bateria | sem erro | shell: nenhuma linha nova de erro; kernel: sem OOM, segfault ou trava; 15 reinícios do shell, todos com código 0 (as instalações dos testes) |

### Como repetir as medidas

```sh
# no aparelho: estado do player e medidas da interface
cat /run/bigfoston-tv                                  # relay=1/0, shown, underruns, lost, drops
echo "perfstat reset" > /run/bigfoston/ctl; sleep 20
echo "perfstat rotulo" > /run/bigfoston/ctl; tail -1 /run/bigfoston/perf.txt
echo "perfstat trace" > /run/bigfoston/ctl             # áreas redesenhadas no log do shell
# toque e passos de teste
echo "tap 720 265" > /run/bigfoston/ctl                 # um toque real pelo LVGL
echo "mapdemo 18" > /run/bigfoston/ctl                  # Big GPS: posição de teste
# no PC: o relay
ss -tnpo | grep 48122                                   # conexão e o timer de keepalive
journalctl --user -u bigfoston-tv-relay -f
```

## Checklist

- [x] Modo TV: GPS, tarefas, sons, animações e papel de parede pausados; Bluetooth e telefone só
      sem nada conectado; player com prioridade e protegido.
- [x] Modo GPS: TV desligada (sem órfão), tarefas pausadas; Bluetooth e telefone só sem nada
      conectado; sons de toque, animações e papel de parede quietos; voz e mapa intactos.
- [x] Instantâneo e volta: nada gravado; nota em `/run` para um shell que caia; sinalizador limpo
      no início.
- [x] Etiqueta "Modo performance" sempre visível, marca d'água fixa no topo central, clicável
      (Desativar / Configurar / ✕); por cima do vídeo da TV em janela e em tela cheia; some na hora.
- [x] TV: relay volta sozinho; sem espera pelo PC sem cabo; a interface nunca espera o player;
      vigia do player travado; redesenho −88 %.
- [x] Big GPS: transporte lembrado; contagem de 10 s no transporte e no Iniciar; parar, trocar e
      desligar a qualquer momento.
- [x] 0 avisos de compilação, analisador e shellcheck; testes passando.

## O que ficou de fora ou pendente (com o motivo)

| Item | Situação |
|---|---|
| Governor de CPU e EPP | não existem no MT3351 (sem `cpufreq`); o plano age no que roda |
| zram e swappiness | zram medido e descartado pelo projeto ([../performance/05-zram.md](../performance/05-zram.md)); sem swap, swappiness não age |
| cgroups e `ionice` | o kernel/BusyBox do aparelho não têm; prioridade por `nice` e OOM |
| Bluetooth e telefone pausados com **nada** conectado | regra coberta pelo `test_perfpolicy` (exaustivo). Não exercitada no aparelho para não desconectar o celular do usuário durante os testes. Falta conferir no carro: abrir a TV sem celular e ver `bfphone` e `bluetoothd` pararem depois de 15 s |
| `S10bigfoston` pulando a vez | o shell já cria `/run/bigfoston-quiet`; o script novo está na imagem enviada ao cartão (vale depois de desligar e ligar) |
| Contagem do Iniciar com rotas alternativas chegando | o roteador não achou alternativas para os destinos do teste; coberto pelo código, não exercitado |
| Instalação definitiva | imagem nova (shell e `S10bigfoston`) gerada pelo `firmware/build.sh` e enviada por `tools/bigfoston/linux-update.py stage`, conferida no cartão; entra ao desligar e ligar o aparelho. O `bftvd` novo está no cartão (`bftvd.0.6.5` é o anterior) |
| Busca de canais completa | feita até o fim (`busca_fim`, 7 canais achados dentro de casa, contra 15 da lista anterior); a lista anterior foi devolvida depois do teste |
