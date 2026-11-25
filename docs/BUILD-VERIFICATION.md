# Beta 2 build verification

Release: v0.3.0-beta.2

Qualification date: 2026-11-24

## Hardware qualification

Result: PASS (on-console testing)

The release candidate was tested for:

- Native application startup
- Removal of the unsupported CONTROLS menu
- Options navigation and settings
- Gameplay
- Audio
- Pause and resume
- FPS display
- Local two-player operation
- Exit and relaunch

## Independent builds

Two clean builds in different directories on
the reference host produced identical application
files, native ELF and SDL2 archive.

All 77 native engine compilation records matched
their reference object checksums.

No engine compiler warnings were reported.

## SHA-256 reference values

Native application executable:

    a805ad4744762b5744e1ebfb17977b2f3a66a50b7ae89097a214af749e2bb0c7

Converted native ELF:

    563de1e3acdeb75fc8b8187040e4f17d53122b06b39667ea84521fad2027f6d5

Portable SDL2 archive:

    9e89b1059b1d9650d0b4396a3c23bdcba982ff5b066d83bc28840f7d2657bf50

Native runtime shim:

    e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036

The original hardware-qualified application files
are preserved independently of publication tooling
and documentation.

Reproducibility across other host distributions
and compiler versions is not asserted.

## GNU libiconv reconstruction

GNU libiconv 1.17 was rebuilt from its verified
upstream source using the pinned PS5 SDK.

Although the rebuilt static archive differed
from the original PacBrew archive, substituting
it produced the exact qualified RC2 executable.

## Modified-library verification

A separate diagnostic build changed the library's
_libiconv_version value from 0x00000111 to
0x1234ABCD.

The modified value was verified directly in the
linked ELF. The resulting executable had a
different SHA-256 fingerprint from RC2.

All other native application distribution
files remained identical.

The diagnostic build was not deployed and is
not part of the release.

See RELINKING.md for the tested rebuilding
and relinking procedure.

## Distribution verification

A clean extraction of the corresponding-source
release candidate reproduced all 77 reference
engine objects, the SDL2 archive, native runtime
and qualified application executable.

All six native application files matched the
player installation archive.

Final publication cleanup removes unrelated
historical attribution and normalizes platform
source naming.

Eight existing compiled SDL2 diagnostic strings
are deliberately retained to preserve the
qualified executable's byte identity.

The official release remains the original
hardware-qualified RC2 application.

## v1.0.0 public release promotion

The hardware-qualified Beta 2 RC2 implementation is promoted
to v1.0.0 for its first stable public release.

The application executable, runtime module, icon and both
background images remain byte-identical to qualified RC2.

The PS5 contentVersion field is updated from 00.003.000
to 01.000.000. The title ID remains PPSA98538.

The original development qualification records are retained.
Historical Beta 2 references describe the tested development
milestone rather than the public release version.

The v1.0.0 metadata promotion is verified separately from
the original RC2 hardware qualification.


## Final v1.0.0 in-game branding correction

The initial v1.0.0 promotion retained the Beta 2 RC2
executable. Final inspection identified obsolete version
text in the Options and About menus.

Three string occurrences in
`engine/upstream/console/menu.c` were corrected to
display V1.0.0.

The engine reference manifest was updated accordingly.

The corrected candidate was compared against the
previous qualified build:

- Engine objects: 77
- Identical engine objects: 76
- Changed engine objects: 1 (menu.c.o)
- Compiler warnings: 0
- Identical native application files: 5 of 6
- Changed application files: eboot.bin only

The runtime module, application metadata and artwork
remain byte-identical to the earlier v1.0.0 promotion.

### Executable fingerprints

Historical RC2 signed executable:

`a805ad4744762b5744e1ebfb17977b2f3a66a50b7ae89097a214af749e2bb0c7`

Final v1.0.0 signed executable:

`4c72f249aaf8408e4ab9446052bb0e0bfd13713ef32a3ac44820c219607a42a8`

Final v1.0.0 raw executable:

`d97df6016a74d412a860d8100d0364ecc4acd62646721477e04e347ed2192282`

Final menu object:

`4e7e88e44d23772826347a89d8f90b2d656571f39c8e773891bd102c49662fea`

The historical RC2 executable and frozen development
starter remain preserved separately.

The normal public build uses the corrected engine
reference checksum without disabling verification.

The current installation file checksums are recorded
in RELEASE-SHA256SUMS.txt.
