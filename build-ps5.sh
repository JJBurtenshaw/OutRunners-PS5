#!/usr/bin/env bash
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
export BUILD_JOBS=${BUILD_JOBS:-4}
for cmd in clang-18 clang++-18 llvm-ar-18 llvm-ranlib-18 cmake git wget unzip tar make python3 pkg-config sha256sum; do command -v "$cmd" >/dev/null || { echo "Missing host tool: $cmd" >&2; exit 1; }; done
export CXX=$(command -v clang++-18)
export PS5_CLANG=$(command -v clang-18)
[[ -f .env ]] || cp .env.example .env
bash tools/setup-native-dependencies.sh
bash tools/setup-pacbrew-dependencies.sh --all
bash tools/build-sdl2.sh
python3 tools/build-engine.py --verify-reference
bash tools/rebuild-libc.sh
make app
printf '%s\n' 'Local build complete. No automatic deployment.'
