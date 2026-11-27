# Etapa 2 — Modos TV e GPS, energia, desempenho e eficiência

## Uma máquina de estados, quatro peças

O Plano de energia, os Efeitos visuais, o Modo performance e o novo Plano de eficiência não
viraram quatro mecanismos: o perfui (`src/ui/core/perfui.c`) continua sendo o único dono do
estado.

| Peça | Onde | O que guarda | Muda o quê |
|---|---|---|---|
| Plano de energia | Ajustes > Potência | `settings.conf` (é escolha do usuário) | brilho, escurecer, sons, efeitos, `perf_always`, detalhe do mapa |
| Desempenho (efeitos) | Ajustes > Desempenho | `settings.conf` | animações, papel de parede |
| Modo performance | Ajustes > Modo performance | `settings.conf` (os ajustes) | o que pausar com vídeo, controle, stream ou à mão |
| **Plano de eficiência** (modos TV e GPS) | ligado pela TV e pelo Big GPS | **nada gravado**: vale só enquanto o app está aberto | serviços, tarefas, interface, prioridades, OOM |

O plano de eficiência fica **por cima** dos ajustes do usuário, sem tocar neles. Por isso a
volta é limpa (veja "Instantâneo e volta" abaixo).

## Estados

```
            perfui_tv(true)                       nav: SCR_MAP
  NORMAL ─────────────────────► TV        NORMAL ─────────────► GPS
    ▲   ◄───────────────────────  │          ▲  ◄──────────────  │
    │      perfui_tv(false)       │          │   saiu do mapa    │
    │      ou "Desativar"         │          │   ou "Desativar"  │
    └─────────────────────────────┘          └───────────────────┘
TV e GPS nunca valem juntos: uma tela por vez (sair da TV para o mapa é TV → NORMAL → GPS).
```

Cada modo tem um interruptor em Ajustes > Modo performance > "Modos TV e GPS (plano de
eficiência)": `perf_tv` e `perf_gps`, ligados de fábrica e religados por qualquer plano de
energia (como os outros motivos automáticos). "Desativar" pela etiqueta ou em Ajustes vale até
fechar o app (`tv_suppressed`, `gps_suppressed`).

Por baixo, o `perfmode` continua com os seus estados de serviço (`PM_OFF → PM_ENTERING → PM_ON →
PM_LEAVING`), numa thread que persegue o último pedido. Mudar de ideia no meio de uma troca
emenda a contrária. Nada disso mudou.

## As regras (src/ui/core/perfpolicy.c)

| | Modo TV | Modo GPS |
|---|---|---|
| GPS (`bfgps`) | **pausa** | fica (o mapa usa) |
| Tarefas agendadas (`crond`) | **pausa** | **pausa** |
| Telefone (`bfphone`) | pausa **só se o celular não estiver conectado** | igual |
| Bluetooth (chip, `bluetoothd`, `bluealsa`, agente) | pausa **só se nada estiver conectado** | igual |
| TV | é o app | **desligada**: `bftvd` órfão encerrado, sintonizador desligado |
| Sons de toque, animações | desligados | desligados (a voz e o movimento do mapa não usam estes) |
| Papel de parede | liso | liso (o mapa cobre a tela; não desenha por baixo) |
| Relatório periódico do sistema | pula a vez | pula a vez |
| Prioridade | `bftvd` nice −10 e OOM −1000; interface −5 | interface −5 (desenha o mapa) |

**Conectado**, conferido a cada segundo nos arquivos de estado (sem D-Bus no shell):
- **Bluetooth conectado:** algum aparelho com `connected=1` em `/run/bigfoston-bt` (celular,
  fone, teclado ou controle) ou o teclado Bluetooth do `hwkbd`.
- **Celular conectado:** um aparelho conectado com ícone `phone` (o que o BlueZ informa) **ou** o
  canal de chamadas aberto (`link=1` em `/run/bigfoston-phone`). Celular conectado segura o
  telefone **e** o Bluetooth, porque as chamadas chegam por ele.

**Carência de 15 s:** nos primeiros 15 s de um modo, o que ainda está rodando não pausa. Quem
acabou de entrar no carro e abriu o GPS dá tempo ao celular de conectar sozinho. A carência não
religa o que já estava pausado (um Bluetooth pausado leva ~6 s para subir).

O app aberto continua segurando o que usa (Telefone, Bluetooth, GpsInfo), como antes.

### Por que o telefone mudou

Antes, abrir a TV pausava o `bfphone` com o POCO X4 conectado. Testado agora no aparelho
(POCO conectado, `link=1`), com a TV aberta 15 s:
- `bfgps` e `crond` pararam;
- `bluetoothd`, `bluealsa`, `bfbt-agent` e `bfphone` continuaram com os mesmos PIDs.

## Interface

O que "Interface" quer dizer no BigFoston (Ajustes > Modo performance > Interface):
- sons de toque e animações (`fx_quiet`);
- papel de parede (`app_wallpaper_plain`);
- prioridade da interface;
- etiqueta;
- janela de progresso.

Nos modos:
- **sons de toque e animações:** desligados, independentemente de `perf_quiet`;
- **papel de parede:** liso, independentemente de `perf_wall`;
- **janela de progresso:** não aparece (já só aparecia com vídeo e "Ativar agora");
- **etiqueta:** sempre visível (etapa 5).

**A prioridade da interface fica em −5 também na TV.** A primeira versão a deixava em 0 ("tudo
da Interface desligado") e a medida desfez a ideia:

| Interface na TV sem o PC (Globo, 20 s) | Laço p95 | Pior volta (toque esperando) | Quadros |
|---|---|---|---|
| nice 0 | 190 ms | **618 ms** | 15 qps |
| nice −5 | 70 ms | 141 ms | 13 qps |

A diferença de quadros está dentro da variação de uma amostra. Já 0,6 s para responder a um
toque é uma tela travada. Ociosa, a interface não gasta nada; a prioridade só pesa quando há um
toque para atender. O `bftvd` (−10, threads de −12 a −15) continua acima.

## Desempenho e energia

- **CPU:** sem `cpufreq`, não há governor nem EPP (468 MHz fixos). O que se ganha é tirando
  trabalho:
  - `bfgps` (9,9 % parado);
  - relatório periódico (1,5 %);
  - redesenho da interface na TV (−88 %, etapa 3).
- **Memória:** sem pressão. TV com o shell: 55–60 MB usados de 119 MB, `MemFree` acima de 60 MB.
  O zram segue desligado ([../performance/05-zram.md](../performance/05-zram.md)): comprimir custa
  0,3–0,8 ms por página neste ARM. O que existe contra falta de memória é proteger os processos
  críticos (OOM −1000 no `bftvd` no modo TV e na interface com prioridade).
- **E/S:** o cartão SD só é escrito na gravação de TV (~340 kbit/s) e nos ajustes. Nenhum ajuste
  de `vm.dirty_*` foi feito: sem uma medida que mostrasse ganho, mudar o writeback é risco sem
  prova.

## Instantâneo e volta (snapshot e rollback)

| O que | Como volta | Se o shell travar ou reiniciar | Se o aparelho reiniciar |
|---|---|---|---|
| Serviços pausados | `perfmode` religa na ordem inversa | a nota `/run/bigfoston-perfmode` (escrita **antes** de parar) faz o próximo shell religar tudo | `/run` é tmpfs: tudo sobe normal no boot |
| Sons, animações, papel de parede | estado em memória (`fx_quiet`, `app_wallpaper_plain`) | o shell novo começa sem eles | idem |
| Prioridades e OOM | voltam a 0 ao sair do modo | processo novo, prioridade normal | idem |
| `/run/bigfoston-quiet` | apagado ao sair do modo | apagado em `perfui_init` | `/run` some |
| Ajustes do usuário | nunca alterados pelos modos | — | — |

Testado no aparelho: sair da TV e do mapa apaga a nota e o sinalizador, e `bfgps` e `crond`
voltam. 6 alternâncias TV ↔ GPS seguidas:
- o shell continuou o mesmo processo;
- a memória variou 16 KB (7472 → 7488 KB);
- nenhum `bftvd` ficou no mapa;
- no fim, o sintonizador estava `parado`.

## Tarefas periódicas do sistema

`/run/bigfoston-quiet` existe enquanto um modo vale. O relatório "alive" do `S10bigfoston`
(`firmware/.../init.d/S10bigfoston`) pula a vez enquanto ele existir: são ~6 processos a cada
10 s, 1,5 % da CPU medido no aparelho. A mudança no script vale a partir da próxima imagem do
firmware (o rootfs fica na memória interna). O shell já cria o sinalizador.
