#!/usr/bin/env bash
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
SDK="$root/.deps/native/ps5-payload-sdk"
PORTS="$root/.deps/pacbrew/v0.40.2/sysroot/user/homebrew"
REF="$root/third-party/sdl2"
EXPECTED=9e89b1059b1d9650d0b4396a3c23bdcba982ff5b066d83bc28840f7d2657bf50
printf '%s  %s\n' 87308eb1558b0219a74e3d94cd57d554d64a968d873ed81d5346baa7ea39076f "$REF/ps5-native.patch" 7e6e6a127eb2480a05c698778c174e444440de9875affa024289e30c8f9f8904 "$REF/cmake-options.txt" | sha256sum -c -
(cd "$PORTS"; sha256sum -c "$REF/iconv-SHA256SUMS.txt")
if [[ -f "$root/local-libs/libSDL2.a" ]] && printf '%s  %s\n' "$EXPECTED" "$root/local-libs/libSDL2.a" | sha256sum -c --status; then echo 'SDL2 cache verified'; exit 0; fi
mkdir -p "$root/.deps" "$root/local-libs"
work=$(mktemp -d "$root/.deps/sdl2-build-XXXXXX")
SRC="$work/source"; BUILD="$work/build"
export PS5_PAYLOAD_SDK="$SDK" PATH="$SDK/bin:$PATH"
mapfile -t opts < <(grep -E '^-D' "$REF/cmake-options.txt")
git clone --no-checkout https://github.com/ps5-payload-dev/SDL.git "$SRC"
git -C "$SRC" checkout --detach ee4c47dc0d617b3bc8f35108f9956baf228a1322
git -C "$SRC" config core.abbrev 7
git -C "$SRC" apply "$REF/ps5-native.patch"
cmake -S "$SRC" -B "$BUILD" -G 'Unix Makefiles' -DCMAKE_TOOLCHAIN_FILE="$SDK/toolchain/prospero.cmake" "${opts[@]}" "-DCMAKE_C_FLAGS=-I$PORTS/include -ffile-prefix-map=$SRC=." "-DCMAKE_EXE_LINKER_FLAGS=-L$PORTS/lib" > "$work/configure.log" 2>&1 || { tail -80 "$work/configure.log"; exit 1; }
cmake --build "$BUILD" --target SDL2-static --parallel "${BUILD_JOBS:-4}" > "$work/build.log" 2>&1 || { tail -80 "$work/build.log"; exit 1; }
printf '%s  %s\n' "$EXPECTED" "$BUILD/libSDL2.a" | sha256sum -c -
cp "$BUILD/libSDL2.a" "$root/local-libs/libSDL2.a.tmp"
mv "$root/local-libs/libSDL2.a.tmp" "$root/local-libs/libSDL2.a"
echo 'PASS: portable SDL2 archive built from source'
