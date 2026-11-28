# Relay do PC: o jogo decodificado no computador

Com o FOSTON ligado ao PC pelo cabo USB, **o PC é o cliente do Sunshine**: decodifica o vídeo a 60
qps, reduz ao tamanho da tela do FOSTON e manda pela rede USB. O FOSTON só desenha e toca. Sem o
relay, o motor `bfrp` do FOSTON decodifica sozinho (modo direto, ~5 a 19 qps conforme a qualidade,
[06](06-video-decoding.md)).

```
Sunshine (PC) ──H.264 1280×720 60 qps──► bfrp-relay (PC)
                                           decodifica (FFmpeg), corta as faixas pretas,
                                           reduz (SWS_AREA), quantiza, delta, LZ4 HC
FOSTON (bfrp) ◄──quadros + som + eventos───┘
  aplica o delta, desenha, toca; manda à tela os eventos e ao relay o toque, as teclas e o controle
```

## Instalar

```
tools/bigfoston/build-remoteplay.sh       # o motor do FOSTON e firmware/build/remoteplay/bfrp-relay
tools/bigfoston/brp-relay-install.sh      # instala e liga o serviço do usuário (systemd)
journalctl --user -u bigfoston-brp-relay -f
```

- As chaves de pareamento são as do FOSTON (`/data/bigfoston/remoteplay`), copiadas uma vez para
  `~/.config/bigfoston-brp-relay`: o Sunshine reconhece o cliente pelo certificado.
- O relay conecta no FOSTON (192.168.7.2:48120) a cada 0,5 s; nenhuma porta é aberta no PC.
- O protocolo das mensagens está em `src/remoteplay/proto.h`: `K` linha de comando (FOSTON → PC),
  `E` evento (PC → FOSTON), `V` quadro, `A` som, `Q` quadro desenhado, `R` pedir quadro inteiro.

## Formato dos quadros

`relay.c` (`tx_thread`): brilho em 6 bits e cor em 5 (a tela é RGB565), com histerese contra o
quadro anterior; só os pares de linhas que mudaram vão, em XOR; quadro inteiro a cada 60, quando
o tamanho muda ou quando o FOSTON pede; LZ4 HC. Até 2 quadros a caminho sem confirmação.

## Correções de 28/11/2026

Medidas jogando Streets of Rage 4 (PC com Sunshine 2026.914, KWin 6.8).

**Botão perdido (B).** O aperto e a soltura saem do FOSTON com o intervalo certo, mas podiam chegar
ao Sunshine juntos: B apertado 81–232 ms no FOSTON chegava com 6 e 10 ms, e B apertado de novo
10 ms depois de soltar chegava como um aperto só. O jogo lê o controle a cada 16,7 ms e perdia o
botão.
- **Relay** (`padhold.h`, usado por `send_pad` em `bfrp.c`): cada botão fica pelo menos 34 ms
  apertado e 34 ms solto (duas leituras a 60 qps); um aperto que chega durante a espera é guardado
  e enviado depois. Os casos medidos estão em `src/ui/tests/test_padhold.c` (`make -C src/ui test`).
- **Shell** (`gamepad_buttons_take`, `scr_brp.c`): a tela lia o estado a cada 8 ms e perdia um
  apertar-e-soltar inteiro entre duas leituras. Agora um toque vem apertado uma vez e um
  soltar-e-apertar vem solto uma vez; fora da sessão o que acumulou é descartado (o toque do menu
  não vira aperto no PC).
- Medido no controle virtual do PC: menor aperto de 0 ms para 34 ms; 27 apertos do B no FOSTON
  chegavam 24, depois **34 de 34** com os mesmos tempos.

**"A sessão terminou" com a sessão funcionando.** O Sunshine, encerrando a sessão que ficou aberta,
mandou um fim (`TERMINATED 11`) no meio da abertura da nova, que seguiu normal. Um fim antes do
`STARTED` da conexão atual agora só vai para o registro; se a nova falhar de verdade,
`LiStartConnection` devolve o erro (`ERR conexão`).

**O FOSTON não voltava quando o relay reiniciava.** O motor ficava num socket morto: não saía e
não voltava a esperar a porta; só fechando e abrindo o app. Agora:
- keepalive de TCP no socket do relay (queda percebida em ~5 s, mesmo sem o PC fechar a conexão);
- caindo o relay, a tela recebe `TERMINATED relay` e o motor volta a esperar; ao reconectar, manda
  de novo o último `HOST` (o relay novo não sabe o servidor);
- **Atualizar** abre um motor novo se o anterior saiu.

**O último quadro ficava na tela ao encerrar.** A tela redesenhava a interface antes de o motor
tratar o `SHOW 0`, e um quadro desenhado nesse intervalo ficava por cima. A tela redesenha de novo
quando chega o `STOPPED` ou o `TERMINATED`, que o motor só manda depois do `SHOW 0`.

**Queda do relay sem pista.** O relay caiu uma vez (SIGSEGV) no fim de uma sessão sem vídeo, e o PC
não guarda core (`core_pattern`). O relay agora é compilado com `-g` e, numa queda, grava a pilha no
registro do serviço; os endereços viram arquivo e linha com
`addr2line -f -e firmware/build/remoteplay/bfrp-relay <endereço>`. A queda não se repetiu.

**Imagem com som e sem vídeo** era do PC, não do FOSTON: com o KWin 6.8, o Big Remote Play do PC
(Janela do jogo) codificava numa GPU e a tela privada desenhava na outra. Corrigido no
[big-remote-play](https://github.com/ruscher/big-remote-play) (tela privada e Sunshine na GPU
principal).

## Desempenho

O controle Bluetooth passou para o HID do kernel (`UserspaceHID=false`): o `bluetoothd` caiu de 20 %
para 0,1 % da CPU durante o jogo e a imagem desenhada subiu de ~15 para ~39 qps
([jogos e pacotes, etapa 7](../jogos-e-pacotes/07-latencia-e-modo-performance.md)). O modo
performance já liga sozinho com a imagem (`perf_stream`); o plano de energia não muda nada durante
a sessão, e o FOSTON não tem ajuste de frequência da CPU.
