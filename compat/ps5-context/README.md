# PS5 context ABI compatibility

Research source:
https://github.com/mihawk-99/PS5_PayloadSDK

Revision:
88d6e41ec7ee7e76d871ff14cd78b5cc4c8e5989

Original file:
include/freebsd/sys/_ucontext.h

Hardware evidence:
platform/docs/PROBE.md

Correction:
The PS5 context includes 48 additional bytes
between the signal mask and machine context.

The original FreeBSD license notice is retained.

This is a project-local diagnostic header override.
Neither the installed SDK nor the cached native SDK
has been modified.

This adaptation is included in the hardware-qualified Beta 2 application.
