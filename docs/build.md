# Compilando as edições nativas

Requisitos comuns: CMake 3.16+, um compilador com C++17 e o SDL2 (só para as
telas gráficas — o modo `--headless` não precisa de SDL).

## Linux (Ubuntu)

```bash
sudo apt-get install -y build-essential cmake libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bin/mega-stars            # janela SDL
./build/bin/mega-stars --headless # ASCII no terminal
```

## Windows 10 ou superior

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DSDL2_DIR=C:\SDL2\cmake
cmake --build build --config Release
.\build\bin\Release\mega-stars.exe
```

Copie `SDL2.dll` para junto do executável. O binário também roda a partir do
PowerShell com `--server=<host>:8781`.

## macOS 10.13 High Sierra (iMac 2011)

O `CMAKE_OSX_DEPLOYMENT_TARGET` já vem fixado em `10.13`, que é a última versão
suportada por esse hardware.

```bash
brew install cmake sdl2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build -j
```

## Android (Mobile e Pocket Editions)

Use o NDK com o toolchain do próprio Android:

```bash
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=armeabi-v7a \      # J5 Prime e aparelhos antigos
  -DANDROID_PLATFORM=android-21 \  # Android 5.0, cobre o J5 Prime
  -DMEGA_BUILD_PC=OFF -DMEGA_BUILD_MOBILE=ON -DMEGA_BUILD_POCKET=ON \
  -DMEGA_BUILD_TESTS=OFF \
  -DSDL2_DIR=<caminho do SDL2 compilado para Android>
cmake --build build-android -j
```

Para 64 bits (Redmi Note 8 e mais novos) troque `ANDROID_ABI` por `arm64-v8a`.
As bibliotecas geradas entram em um projeto Gradle padrão do SDL2
(`android-project/app/jni`), que empacota o `.apk`.

## iOS / iPadOS

```bash
cmake -S . -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \   # iPad 9 e iPhone 6S+
  -DMEGA_BUILD_PC=OFF -DMEGA_BUILD_MOBILE=ON -DMEGA_BUILD_POCKET=ON \
  -DMEGA_BUILD_TESTS=OFF
cmake --build build-ios --config Release
```

Para o iPhone 6 (iOS 12 é a última versão dele) compile só a Pocket Edition:
ela usa 30 fps, arena menor, sem partículas e simulação a 15 Hz.

## Epic Online Services

```bash
cmake -S . -B build -DMEGA_EOS_SDK=/caminho/EOS-SDK
```

O CMake procura `SDK/Include/eos_sdk.h` e a biblioteca em `SDK/Lib` ou
`SDK/Bin`, define `MEGA_WITH_EOS` e habilita `--server=eos:<lobby>`.
Sem o SDK o jogo continua compilando e usa o transporte TCP.
