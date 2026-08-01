#!/usr/bin/env bash
# Builds the Mac version of the PC Edition (and optionally Mobile/Pocket).
# Works on High Sierra 10.13 (iMac 2011) and newer.
set -euo pipefail

cd "$(dirname "$0")/.."

if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew nao encontrado. Instale em https://brew.sh e rode de novo." >&2
    exit 1
fi

brew list cmake >/dev/null 2>&1 || brew install cmake
brew list sdl2 >/dev/null 2>&1 || brew install sdl2

cmake -S . -B build-macos \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DMEGA_BUILD_MOBILE=ON \
    -DMEGA_BUILD_POCKET=ON
cmake --build build-macos -j"$(sysctl -n hw.ncpu)"

echo
echo "Pronto. Executaveis em build-macos/bin:"
ls -1 build-macos/bin
echo
echo "Para jogar:  ./build-macos/bin/mega-stars --server=SEU-SERVIDOR:8781"
