# Rebuilding and relinking GNU libiconv

OutRunners-PS5 statically links GNU libiconv 1.17.

The library is available under LGPL-2.1-or-later.
Recipients may modify the library and rebuild
or relink the application with their modified version.

The normal reference build verifies pinned
dependency checksums. These checks establish
release reproducibility and are not restrictions
on permitted modification.

## Prepare a reference build

Use a separate working copy of the project.

From its repository root:

    bash build-ps5.sh

This prepares the native SDK, PacBrew dependencies,
SDL2 archive, engine objects and runtime module.

## Obtain the library source

The corresponding-source archive accompanying
the release contains:

- GNU libiconv 1.17 source archive
- Original GNU license texts
- Pinned PacBrew build recipe

Extract libiconv into a separate working directory
and make any desired source modifications.

## Build the replacement library

From the project root, record its absolute path:

    export PROJECT_ROOT="$PWD"
    export PS5_PAYLOAD_SDK="$PROJECT_ROOT/.deps/native/ps5-payload-sdk"
    export PATH="$PS5_PAYLOAD_SDK/bin:$PATH"

Enter your extracted libiconv source directory,
then initialize the PS5 cross-compiler:

    source "$PS5_PAYLOAD_SDK/toolchain/prospero.sh"

Configure and build:

    ./configure \
        --prefix=/user/homebrew \
        --host=x86_64-pc-freebsd \
        --enable-static \
        --disable-shared

    make -j4

Install into a temporary local staging directory:

    INSTALL="$(mktemp -d)"
    make install DESTDIR="$INSTALL"

## Replace and relink

Use the separate application working copy,
not an archived official release.

    cd "$PROJECT_ROOT"

    PORTS="$PROJECT_ROOT/.deps/pacbrew/v0.40.2/sysroot/user/homebrew"

Back up the original library:

    cp "$PORTS/lib/libiconv.a" \
       "$PORTS/lib/libiconv.a.original"

Install the replacement:

    cp "$INSTALL/user/homebrew/lib/libiconv.a" \
       "$PORTS/lib/libiconv.a"

For an ABI-compatible library modification,
the existing SDL2 archive and headers can
remain unchanged.

Invoke the final build stage directly:

    CXX="$(command -v clang++-18)" \
    PS5_CLANG="$(command -v clang-18)" \
    PS5_AUTO_DEPLOY=0 \
    make app

This avoids the reference SDL2 reconstruction
step that expects the original libiconv checksum.

The new application is assembled in:

    dist/PPSA98538/

Do not expect a modified application to match
the official release checksum.

ABI-changing library modifications may also
require rebuilding SDL2 against the modified
headers and adjusting reference checksum checks
in the developer's own build scripts.

No automatic console deployment is performed.

## Verification

An unmodified GNU libiconv 1.17 reconstruction
produced a byte-identical copy of the qualified
Beta 2 executable.

A separate diagnostic build changed the library's
version variable and verified the modified value
inside the linked application ELF.

Neither test changed the official release files.

These tests establish local rebuilding and relinking
on the reference development environment. They do
not assert hardware qualification of arbitrary
modified applications.
