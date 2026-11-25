# Third-party notices

OutRunners-PS5 incorporates and builds upon
software from several independent open-source
projects.

The applicable copyright and license notices
remain in their respective source files and
component license documents.

## OutRunners

Upstream native port and decompilation by
Sebastian Toyos and contributors.

Upstream version: v1.3.2

Revision:
4396d7119f0a30404b0bf4caf047c188c365a9f8

License: BSD-3-Clause.

https://github.com/cheviman/outrunners-port

The included engine source contains PS5-specific
adaptations.

See engine/upstream/LICENSE and
engine/upstream/lan/LICENSE.

## PS5 native application foundation

Original ps5-native-app-boilerplate code:

Copyright (C) 2026 BlackBearReloaded.

License: GPL-3.0-or-later.

Original copyright and SPDX notices are retained.

The native runtime shim and application
conversion tooling are derived from this
open-source foundation.

## PS5 Payload SDK

Version: v0.42

Revision:
4eb701204fc3f8d31e84cf8ca272974e2be9c867

https://github.com/ps5-payload-dev/sdk

The SDK generally uses GPL-3.0-or-later,
with BSD-licensed FreeBSD components and
other explicitly identified exceptions.

Individual component notices apply.

## PacBrew

Version: v0.40.2

Revision:
1687e827675c66e1b040751c7f0811222bd3aea0

https://github.com/ps5-payload-dev/pacbrew-repo

PacBrew supplies the pinned cross-compiled
dependency environment. Individual libraries
retain their respective licenses.

## SDL2

Copyright (C) Sam Lantinga and SDL contributors.

License: Zlib.

Version: 2.30.12 with PS5-specific modifications.

PS5 upstream revision:
ee4c47dc0d617b3bc8f35108f9956baf228a1322

https://github.com/ps5-payload-dev/SDL.git

The included SDL2 patch is a modified source
version and is not represented as unmodified
upstream SDL2.

See third-party/sdl2/.

## GNU libiconv

Version: 1.17

Copyright Free Software Foundation, Inc.
and respective contributors.

Library license: LGPL-2.1-or-later.

https://www.gnu.org/software/libiconv/

GNU libiconv is statically linked into the
native OutRunners-PS5 application.

The accompanying corresponding-source package
includes the pinned upstream library source,
applicable license texts and build recipe.

The application can be rebuilt and relinked
against an ABI-compatible modified library.
See docs/RELINKING.md for the tested procedure.

## LLVM runtime libraries

Version identified by the pinned SDK: 18.1.8.

Relevant components include libc++, libc++abi
and libunwind.

License: Apache-2.0 WITH LLVM-exception,
subject to individual source notices.

https://github.com/llvm/llvm-project

## zlib

Version: 1.3.2

Copyright Jean-loup Gailly and Mark Adler.

License: Zlib.

https://zlib.net/

Used by the native application build tooling.

## libco

Revision:
e18e09d634d612a01781168ad4d76be10a7e3bad

https://github.com/higan-emu/libco

License: ISC, with a separate BSD notice
in valgrind.h.

The AMD64 implementation has been adapted
for the native PS5 environment.

See src/orunners_libco/LICENSE.

## FreeBSD context compatibility

Source reference:
https://github.com/mihawk-99/PS5_PayloadSDK

Revision:
88d6e41ec7ee7e76d871ff14cd78b5cc4c8e5989

The original FreeBSD copyright and license
notice is retained in the adapted header.

See compat/ps5-context/.

## Presentation artwork

The PS5 icon and background images were
AI-generated specifically for OutRunners-PS5.

See docs/ARTWORK.md.

## Original arcade game

OutRunners and its original game data belong
to their respective rights holders.

No arcade ROMs or extracted game asset packs
are provided.

This is an unofficial project and is not
endorsed by Sega or Sony Interactive Entertainment.

## Corresponding source

The corresponding-source archive contains
the application source, dependency sources,
build recipes, patches and license texts.

See docs/DEPENDENCY-SOURCES.md and
docs/RELINKING.md for source and rebuilding
information.

The complete GPL and LGPL license texts
and applicable component notices accompany
the release.

## libsamplerate

Version: 0.2.2.

https://github.com/libsndfile/libsamplerate

BSD license; see the accompanying original
COPYING file.

The library appears in the static linker
configuration. Its implementation was not
identified by the selected RC2 symbol checks.

Its pinned source and recipe are included
conservatively in the source distribution.

## Historical notices

docs/ORIGINAL-NOTICES.md preserves the inherited
native application foundation's original notices.

Statements concerning that foundation's original
BlackBear artwork, selection audio and optional
build features do not describe the current
OutRunners-PS5 release contents.

The current presentation artwork is documented
in docs/ARTWORK.md.
