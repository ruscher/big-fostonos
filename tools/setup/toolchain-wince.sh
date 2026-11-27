#!/bin/sh
# toolchain-wince.sh — monta, dentro deste repositório, o compilador do agente do Windows CE
# (src/wince/agent -> bfagent.exe): CeGCC 0.59.1 (arm-mingw32ce) e as duas bibliotecas de
# 32 bits que ele precisa e que não existem mais nos repositórios (GMP 4.3.2 e MPFR 2.4.2).
#
#   tools/setup/toolchain-wince.sh        -> toolchain/opt/mingw32ce e toolchain/deps32
#
# Tudo é baixado das fontes oficiais e conferido por SHA-256. Precisa de: curl, tar, bzip2,
# make e um gcc com suporte a -m32 (no Arch/Manjaro/BigLinux: lib32-gcc-libs e gcc-multilib
# ou o gcc atual, que já gera 32 bits com as lib32-*).
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
R=$(cd "$(dirname "$0")/../.." && pwd)
T=$R/toolchain
SRC=$T/deps-src
J=$(nproc 2>/dev/null || echo 4)
mkdir -p "$T" "$SRC"

fetch() {   # url arquivo sha256
    if [ ! -f "$2" ] || ! echo "$3  $2" | sha256sum -c --quiet - 2>/dev/null; then
        echo "baixando $(basename "$2")"
        curl -fL --retry 3 -o "$2.part" "$1"
        mv "$2.part" "$2"
    fi
    echo "$3  $2" | sha256sum -c --quiet - || { echo "SHA-256 diferente: $2"; exit 1; }
}

fetch https://downloads.sourceforge.net/project/cegcc/cegcc/0.59.1/mingw32ce-0.59.1.tar.bz2 \
      "$T/mingw32ce-0.59.1.tar.bz2" dd7e08204330323fb392b83aeb16abdf9d165eeebd9bcb93dc7f9560b8027470
fetch https://ftp.gnu.org/gnu/gmp/gmp-4.3.2.tar.bz2 \
      "$SRC/gmp-4.3.2.tar.bz2" 936162c0312886c21581002b79932829aa048cfaf9937c6265aeaa14f1cd1775
fetch https://ftp.gnu.org/gnu/mpfr/mpfr-2.4.2.tar.bz2 \
      "$SRC/mpfr-2.4.2.tar.bz2" c7e75a08a8d49d2082e4caee1591a05d11b9d5627514e678f02d66a124bcf2ba

if [ ! -x "$T/opt/mingw32ce/bin/arm-mingw32ce-gcc" ]; then
    echo "extraindo o CeGCC"
    tar -C "$T" -xjf "$T/mingw32ce-0.59.1.tar.bz2"
fi

# GMP e MPFR antigos não compilam com os avisos que viraram erro no GCC 14+: rebaixados de novo
FL="-m32 -O2 -std=gnu89 -Wno-error=implicit-function-declaration -Wno-error=implicit-int \
-Wno-error=incompatible-pointer-types -Wno-error=int-conversion"
D=$T/deps32
if [ ! -f "$D/lib/libgmp.so.3" ]; then
    rm -rf "$SRC/gmp-4.3.2"
    tar -C "$SRC" -xjf "$SRC/gmp-4.3.2.tar.bz2"
    (cd "$SRC/gmp-4.3.2" && ABI=32 CFLAGS="$FL" ./configure --prefix="$D" --build=i686-pc-linux-gnu \
        --disable-static > "$SRC/gmp-configure.log" && make -j"$J" > "$SRC/gmp-make.log" 2>&1 &&
        make install > "$SRC/gmp-install.log") || { echo "GMP falhou: veja $SRC/gmp-*.log"; exit 1; }
fi
if [ ! -f "$D/lib/libmpfr.so.1" ]; then
    rm -rf "$SRC/mpfr-2.4.2"
    tar -C "$SRC" -xjf "$SRC/mpfr-2.4.2.tar.bz2"
    (cd "$SRC/mpfr-2.4.2" && CFLAGS="$FL" ./configure --prefix="$D" --build=i686-pc-linux-gnu --with-gmp="$D" \
        --disable-static > "$SRC/mpfr-configure.log" && make -j"$J" > "$SRC/mpfr-make.log" 2>&1 &&
        make install > "$SRC/mpfr-install.log") || { echo "MPFR falhou: veja $SRC/mpfr-*.log"; exit 1; }
fi

LD_LIBRARY_PATH=$D/lib "$T/opt/mingw32ce/bin/arm-mingw32ce-gcc" --version | head -1
echo "pronto: make -C src/wince/agent"
