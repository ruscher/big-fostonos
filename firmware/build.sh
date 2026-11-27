#!/bin/sh
# build.sh — compila o BigFoston OS (kernel, initrd e DTB) com o Buildroot, do zero.
#
#   firmware/build.sh                 build de desenvolvimento (põe a sua chave SSH pública no root)
#   firmware/build.sh --release       build público: sem chave SSH nenhuma; no fim monta o pacote
#                                     dist/BigFoston-OS-FS707DC-<versão>.zip para o instalador
#   firmware/build.sh --release --notes "texto"   o mesmo, com as notas da versão no manifest.json
#   firmware/build.sh --config bigfoston_web_defconfig --out out-web   outras configurações
#   firmware/build.sh -- menuconfig   repassa alvos ao make do Buildroot
#
# Baixa o Buildroot 2026.02.3 de buildroot.org (SHA-256 conferido) para firmware/build/; os
# pacotes-fonte ficam em firmware/build/dl (ou em $BR2_DL_DIR). Saída: firmware/build/<out>/images/
# bigfoston/{zImage,initrd.img,fs707dc.dtb,SHA256SUMS}. A primeira vez leva de 1 a 2 horas e uns
# 12 GB de disco. Dependências do Buildroot: https://buildroot.org/downloads/manual/manual.html#requirement
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
B=$R/firmware/build
BR_VER=2026.02.3
BR_SHA=5a59e7501b0b4ec52c41f4bfa79412320e0b37eae5f719605a258e8d0c6fc7fb
CONFIG=bigfoston_fs707dc_defconfig
OUT=out
RELEASE=0
NOTES=
while [ $# -gt 0 ]; do
    case $1 in
        --release) RELEASE=1 ;;
        --notes) NOTES=$2; shift ;;
        --config) CONFIG=$2; shift ;;
        --out) OUT=$2; shift ;;
        --) shift; break ;;
        -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
        *) echo "opção desconhecida: $1 (veja --help)"; exit 2 ;;
    esac
    shift
done

mkdir -p "$B"
export BR2_DL_DIR=${BR2_DL_DIR:-$B/dl}
mkdir -p "$BR2_DL_DIR"
TAR=$BR2_DL_DIR/buildroot-$BR_VER.tar.xz
if [ ! -f "$TAR" ] || ! echo "$BR_SHA  $TAR" | sha256sum -c --quiet - 2>/dev/null; then
    echo "baixando o Buildroot $BR_VER"
    curl -fL --retry 3 -o "$TAR.part" "https://buildroot.org/downloads/buildroot-$BR_VER.tar.xz"
    mv "$TAR.part" "$TAR"
fi
echo "$BR_SHA  $TAR" | sha256sum -c --quiet - || { echo "SHA-256 do Buildroot diferente"; exit 1; }
[ -f "$B/buildroot-$BR_VER/Makefile" ] || tar -C "$B" -xJf "$TAR"

if [ "$RELEASE" = 1 ]; then
    # nunca a chave de quem compila numa imagem pública (daria root em todo FOSTON instalado)
    export BIGFOSTON_SSH_PUBKEY=/dev/null/x BIGFOSTON_SSH_HOSTKEY=/dev/null/x
fi
BR="make -C $B/buildroot-$BR_VER O=$B/$OUT BR2_EXTERNAL=$R/firmware/buildroot"
[ -f "$B/$OUT/.config" ] || $BR "$CONFIG"
if [ $# -gt 0 ]; then
    $BR "$@"
    exit 0
fi
$BR
echo "imagens: $B/$OUT/images/bigfoston"

if [ "$RELEASE" = 1 ] && [ "$OUT" = out ]; then
    make -C "$R/src/wince/agent" all imports
    mkdir -p "$R/dist"
    python3 "$R/installer/tools/make_release.py" --zip --notes "$NOTES" "$R/dist"
fi
