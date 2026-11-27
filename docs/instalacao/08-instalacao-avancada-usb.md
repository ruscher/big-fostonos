# 8 · Instalação avançada pelo cabo USB

[⬅ Solução de problemas](07-solucao-de-problemas.md) · [Índice](README.md) · [Próximo: informações técnicas ➡](09-informacoes-tecnicas.md)

É o caminho de linha de comando que o projeto usou no aparelho real desde o agente 0.4.0:
o PC fala com o agente pela porta serial USB do Windows CE (protocolo BFP) e grava a memória
interna arquivo por arquivo, relendo e conferindo cada um. Serve para desenvolvedores e como
alternativa se o [“Instalar o Dual Boot”](04-instalar-no-foston.md) não terminar.

## Requisitos

- Linux com Python 3 e `fdtput` (pacote `dtc`).
- Cabo **mini-USB de dados**.
- O pacote da versão descompactado (`BigFoston-OS-FS707DC-<versão>/`).
- No PC, se o Big Screen Monitor estiver instalado, ele também abre a porta do FOSTON:
  pare o serviço durante a instalação, ou repita o comando que acusar porta ocupada.
- Recomendado: [`tools/pc/bigfoston-ipaq.conf`](../../tools/pc/bigfoston-ipaq.conf) em
  `/etc/modprobe.d/` (evita até 100 s de espera do driver `ipaq` ao trocar de modo).

## Passo a passo

1. Copie `bfagent.exe` para `BigFoston\` no cartão (leitor de cartões) e aponte
   **Ajustes → Caminho da navegação** para ele (como no [passo 5](04-instalar-no-foston.md#passo-a-passo)).
2. Toque em **Navegação** e ligue o cabo USB no PC. O agente troca o USB para porta serial
   (`045e:00ce`, `/dev/ttyUSB0` no PC) e mostra “Pronto na porta COM6”.
3. No PC, na raiz deste repositório:

```bash
P=~/Downloads/BigFoston-OS-FS707DC-0.6.5
B=tools/bigfoston/bfagent.py
python3 $B info                                   # confere o aparelho (WinCE 5.0, APOLLO, MTK, 800x480)
python3 $B install-agent --exe $P/bfagent.exe     # \ResidentFlash\BigFoston\bfagent.exe (SHA-256 conferido)
python3 $B install-linux --dir $P/linux           # zImage, initrd.img, fs707dc.dtb + SHA256SUMS
python3 $B autostart on --exe '\ResidentFlash\BigFoston\bfagent.exe'   # Launch90 + Depend90 = 14 00 32 00
python3 $B autoboot on --delay 12                 # contagem de 12 s ao ligar
python3 $B quit
```

> ⚠️ O `--exe` é obrigatório aqui: sem ele o `autostart on` aponta para o agente **do cartão**
> (`\SDMMC\BigFoston\bfagent.exe`), e o agente só faz a contagem do Dual Boot quando é aberto
> da memória interna.

4. Desligue o cabo, desligue e ligue o FOSTON: a tela Dual Boot conta 12 s e inicia o
   BigFoston OS.

> ⚠️ **Não inicie o Linux com o cabo USB ligado** pelo `boot-mem`: o DMA da função USB do CE
> continua rodando e corrompe a RAM. O `boot-flash` e o boot automático já fecham isso antes.

## Outros comandos úteis

| Comando | Para quê |
|---|---|
| `bfagent.py boot-flash --dtb $P/linux/fs707dc.dtb` | inicia o Linux da memória interna agora, levando a hora do PC |
| `bfagent.py settime` | acerta o relógio do Windows CE pelo PC |
| `bfagent.py shot tela.png` | captura a tela do FOSTON |
| `bfagent.py usb-default mass` | o aparelho volta a ligar como pendrive |
| `bfagent.py ramoops` | o log do kernel do último boot (depois de um reset) |
| `bfagent.py ls '\ResidentFlash\BigFoston\linux'` | o que está instalado |

## Desfazer pelo cabo USB

```bash
python3 tools/bigfoston/bfagent.py autostart off      # apaga Launch90/Depend90
python3 tools/bigfoston/bfagent.py autoboot off
python3 tools/bigfoston/bfagent.py rm '\ResidentFlash\BigFoston\bfagent.exe'
```

Detalhes do protocolo e do carregador: [05-bootloader.md](../05-bootloader.md).
