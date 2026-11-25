# Building OutRunners-PS5

## Reference environment

The qualified v1.0.0 build was produced using:

| Component | Version |
|---|---|
| Host OS | Ubuntu 24.04 / WSL2 |
| Clang | 18.1.3 |
| CMake | 3.28.3 |
| PS5 Payload SDK | v0.42 |
| PacBrew | v0.40.2 |
| SDL2 | 2.30.12, patched |
| zlib | 1.3.2 |
| GNU libiconv | 1.17 |
| OutRunners upstream | v1.3.2 |

## Host dependencies

The reference build expects the following
commands to be available:

- clang-18
- clang++-18
- llvm-ar-18
- llvm-ranlib-18
- cmake
- git
- wget
- unzip
- tar
- make
- python3
- pkg-config
- sha256sum

## Build

From the repository root:

    bash build-ps5.sh

The build entry point performs the following:

1. Initializes the local build configuration.
2. Retrieves pinned native SDK dependencies.
3. Retrieves the pinned PacBrew dependency environment.
4. Reconstructs the patched SDL2 static archive.
5. Compiles all 77 native engine translation units.
6. Reconstructs the native runtime shim.
7. Compiles, links and assembles the application.

The resulting native application is written to:

    dist/PPSA98538/

The build does not automatically deploy to a console.

## Reproducibility

The reference build verifies selected dependency
and object checksums.

The qualified release was reproduced in two
independent clean build directories on the same
reference host.

Different host compiler versions or build
environments may produce different results.

See docs/BUILD-VERIFICATION.md for reference hashes.

## Upstream revisions

OutRunners v1.3.2:

    4396d7119f0a30404b0bf4caf047c188c365a9f8

SDL2 PS5 upstream:

    ee4c47dc0d617b3bc8f35108f9956baf228a1322

The PS5-specific SDL2 modifications are retained
under third-party/sdl2/.

## Generated files

Dependency caches, compiled engine objects,
static libraries, runtime binaries and application
output are generated locally.

They are not part of the normal source repository.

Game ROMs are not required to compile the application.
They are required to play the game.

## Dependency source and modified-library builds

The separate corresponding-source archive includes
the pinned dependency sources and build recipes.

See:

- [Dependency sources](docs/DEPENDENCY-SOURCES.md)
- [Modified-library relinking](docs/RELINKING.md)
- [Third-party notices](THIRD-PARTY-NOTICES.md)
