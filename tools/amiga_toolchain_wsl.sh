#!/usr/bin/env bash
# =============================================================
# amiga_toolchain_wsl.sh — One-Shot m68k-amigaos-gcc Toolchain
# (WSL2/Ubuntu) + STRUDELFORCE 68k-Binary + ADF.
#
# Nutzung (nach WSL-Installation, im Workspace-Root):
#   wsl -d Ubuntu
#   cd /mnt/f/AgentHome/STRUDELFORCE_Master
#   bash tools/amiga_toolchain_wsl.sh
#
# Schritte:
#   [1/4] apt Build-Abhaengigkeiten (make, gmp, mpfr, mpc, flex, ...)
#   [2/4] jeffv03/amiga-gcc bauen: binutils gcc libgcc libnix ixemul
#         (Prefix /opt/amiga, ~10 min; wird per Pfadcheck gecacht)
#   [3/4] make amiga_bin      -> strudelforce.amiga
#   [4/4] make amiga_adf      -> strudelforce.adf (FFS 880K, xdftool)
# =============================================================
set -euo pipefail
cd "$(dirname "$0")/.."

PREFIX="${AMIGA_PREFIX:-/opt/amiga}"
SRC="${AMIGA_GCC_SRC:-$HOME/amiga-gcc-src}"

echo "==> [1/4] apt Build-Abhaengigkeiten..."
sudo apt-get update
sudo apt-get install -y make git gcc g++ lhasa rsync \
     libgmp-dev libmpfr-dev libmpc-dev flex gettext python3-pip

if [ ! -x "$PREFIX/bin/m68k-amigaos-gcc" ]; then
  echo "==> [2/4] m68k-amigaos-gcc Toolchain bauen (~10 min)..."
  if [ ! -d "$SRC" ]; then
    git clone --depth 1 https://github.com/jeffv03/amiga-gcc "$SRC"
  fi
  (cd "$SRC" && make binutils gcc libgcc libnix ixemul \
      -j"$(nproc)" PREFIX="$PREFIX")
else
  echo "==> [2/4] Toolchain existiert unter $PREFIX — uebersprungen."
fi

echo "==> [3/4] strudelforce.amiga (68k) bauen..."
make amiga_bin M68K_CC="$PREFIX/bin/m68k-amigaos-gcc"

echo "==> [4/4] Musik stageen + strudelforce.adf bauen..."
python3 -m pip install --quiet --user amitools 2>/dev/null \
  || python3 -m pip install --quiet --break-system-packages --user amitools
export PATH="$HOME/.local/bin:$PATH"
make amiga_adf M68K_CC="$PREFIX/bin/m68k-amigaos-gcc"

echo ""
echo "OK: strudelforce.adf fertig fuer WinUAE/VAMOS."
echo "    Test: A500 (OCS), ADF als DF0: booten ODER"
echo "    als Daten-Disk:  df1:strudelforce.amiga  aus der Shell."