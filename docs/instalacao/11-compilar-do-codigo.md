# 11 · Compilar do código

[⬅ Dados e apps opcionais](10-dados-e-apps-opcionais.md) · [Índice](README.md)

Tudo o que vai para o aparelho sai deste repositório, sem depender de nenhum outro: o
compilador do Windows CE e o Buildroot são baixados das fontes oficiais, com o SHA-256
conferido, por scripts daqui.

| Peça | Fonte | Como gerar | Saída |
|---|---|---|---|
| Agente do Windows CE | `src/wince/agent/` | `tools/setup/toolchain-wince.sh` + `make -C src/wince/agent` | `build/bfagent.exe` |
| Kernel, initrd e DTB | `firmware/`, `src/{ui,gps,bt,fm,bigscreen,bringup}` | `firmware/build.sh` | `firmware/build/out/images/bigfoston/` |
| Pacote de versão | os dois acima | `firmware/build.sh --release` | `dist/BigFoston-OS-FS707DC-<versão>.zip` |
| Instalador | `installer/` | `cd installer/packaging/arch && makepkg -si` | pacote `bigfoston-installer` |

## 1. Agente do Windows CE (`bfagent.exe`)

```bash
tools/setup/toolchain-wince.sh        # CeGCC 0.59.1 + GMP 4.3.2 + MPFR 2.4.2 (32 bits) em toolchain/ (~1 min)
make -C src/wince/agent               # build/bfagent.exe
make -C src/wince/agent imports       # cada função importada existe no coredll do FOSTON?
make -C src/wince/agent preview       # a tela Dual Boot em PNG, no PC (precisa do wine)
```

- Precisa de `gcc` com suporte a `-m32` (BigLinux/Manjaro: `lib32-glibc`, `lib32-gcc-libs`).
- O `make imports` existe porque o Windows CE do aparelho **não abre, e não avisa**, um `.exe`
  que importa uma função ausente do `coredll.dll` dele (aconteceu com `_vsnwprintf`). A lista
  das 1 778 funções exportadas pelo aparelho está em
  [`src/wince/agent/tools/coredll-exports.txt`](../../src/wince/agent/tools/coredll-exports.txt).
- O binário compilado aqui é **idêntico byte a byte** ao do pacote 0.6.5, exceto o carimbo de
  data do cabeçalho PE (conferido em 27/11/2026).

## 2. Firmware (Linux)

```bash
firmware/build.sh                     # build de desenvolvimento
firmware/build.sh --release           # imagem pública + dist/BigFoston-OS-FS707DC-<versão>.zip
firmware/build.sh -- menuconfig       # repassa alvos ao make do Buildroot
```

- Baixa o **Buildroot 2026.02.3** (SHA-256 conferido) para `firmware/build/`; os pacotes-fonte
  ficam em `firmware/build/dl` (ou em `$BR2_DL_DIR`). A primeira vez leva de 1 a 2 horas e
  ~12 GB de disco. Dependências: as do
  [manual do Buildroot](https://buildroot.org/downloads/manual/manual.html#requirement), mais
  `python3`, `xz` e `dtc` (`fdtput`).
- Configurações: `bigfoston_fs707dc_defconfig` (oficial, com Bluetooth),
  `bigfoston_fs707dc_static_defconfig` (estática, sem Bluetooth),
  `bigfoston_web_defconfig` (bibliotecas do navegador: `--config bigfoston_web_defconfig --out out-web`).
- **SSH:** o build de desenvolvimento autoriza a sua chave `~/.ssh/id_ed25519.pub` como root
  (troque com `BIGFOSTON_SSH_PUBKEY=…`). O `--release` não põe chave nenhuma, e o
  `make_release.py` **recusa** um initrd com `root/.ssh/authorized_keys` ou chave de host fixa.
- A versão vem de `src/ui/Makefile` (`VERSION`). Ao mudar, rode
  `firmware/build.sh -- bigfoston-shell-dirclean` antes (a versão entra por `-D`, que não
  dispara recompilação).

## 3. Pacote de versão

O `--release` já chama, no fim:

```bash
make -C src/wince/agent all imports
python3 installer/tools/make_release.py --zip dist
```

O `make_release.py` confere o `SHA256SUMS` do build, recusa build de outra versão e chaves SSH,
põe o campo `bigfoston,boot-epoch` no DTB, escreve `manifest.json` + `SHA256SUMS` e **reabre o
pacote com o mesmo leitor do instalador** para conferir tudo.

## 4. Instalador

```bash
cd installer
python3 -m pytest -q tests/test_core.py                         # núcleo
xvfb-run -a python3 -m pytest -q tests/test_ui.py               # interface num display virtual
BIGFOSTON_TEST_POLKIT=1 python3 -m pytest -q tests/test_udisks.py   # UDisks2 real (pede a senha)
env -u GTK_THEME xvfb-run -a -s "-screen 0 1280x1024x24" \
    python3 tools/screenshots.py ../dist/BigFoston-OS-FS707DC-0.6.5.zip ../docs/images/instalador
```

## 5. Testes do código do sistema

```bash
make -C src/wince/agent host-test && python3 -m pytest -q tests/agent   # protocolo BFP (C ↔ Python)
python3 -m pytest -q tests/bigscreen                                    # FSP x Big Screen Monitor (pulado sem ../big-screen-monitor-display)
make -C src/ui sim                                                      # a interface no PC (simulador SDL2; baixa o LVGL sozinho)
```

Mais: [porte do Linux](../04-linux-port.md) · [interface](../09-interface-ux-ui.md) ·
[estado da implementação](../implementation-status.md).
