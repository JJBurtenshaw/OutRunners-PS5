# Dependency source references

The separate corresponding-source distribution
contains pinned dependency sources, recipes and
license documents used for v1.0.0.

## PS5 Payload SDK

Version: v0.42

Revision:
4eb701204fc3f8d31e84cf8ca272974e2be9c867

https://github.com/ps5-payload-dev/sdk

## PacBrew

Version: v0.40.2

Revision:
1687e827675c66e1b040751c7f0811222bd3aea0

https://github.com/ps5-payload-dev/pacbrew-repo

## GNU libiconv

Version: 1.17

Source SHA-256:

    8f74213b56238c85a50a5329f77e06198771e70dd9a739779f4c02f65d971313

## libsamplerate

Version: 0.2.2

Source SHA-256:

    3258da280511d24b49d6b08615bbe824d0cacc9842b0e4caf11c52cf2b043893

Included conservatively because the library
appears in the SDL2 static linker configuration.

## LLVM

Version: 18.1.8

Source SHA-256:

    0b58557a6d32ceee97c8d533a59b9212d87e0fc4d2833924eb6c611247db2f2a

## zlib

Version: 1.3.2

Source SHA-256:

    bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16

## SDL2

Version: 2.30.12

PS5 upstream revision:

ee4c47dc0d617b3bc8f35108f9956baf228a1322

The corresponding-source archive includes the
upstream source and the project's PS5 patch.

See third-party/sdl2/ for the build configuration.

## Modified-library builds

See RELINKING.md for the verified local procedure
for rebuilding and replacing GNU libiconv.
