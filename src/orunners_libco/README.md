# libco AMD64 PS5 adaptation

Original project:
https://github.com/higan-emu/libco

Revision:
e18e09d634d612a01781168ad4d76be10a7e3bad

License:
ISC, with separate BSD notice in valgrind.h.

Adaptation:
The original System V AMD64 instruction bytes are
placed in an executable text section by the assembler
rather than made executable at runtime using mprotect.

The original context-switching instruction sequence
and stack initialization algorithm are retained.

This adaptation is included in the hardware-qualified Beta 2 application.
