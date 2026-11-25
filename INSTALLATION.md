# Installation

## Requirements

- PlayStation 5 with a working native homebrew environment.
- A compatible mounting solution, such as ShadowMountPlus.
- Your own legally obtained OutRunners World ROM set
  (MAME set name: orunners).
- Sufficient writable storage for game data extraction
  and the generated asset cache.

No ROMs or extracted arcade game assets are included.

## Quick start

1. Extract the OutRunners-PS5 release archive.
2. Copy the PPSA98538 folder to /data/homebrew/ on your PS5.
3. Place your orunners.zip inside the PPSA98538 folder,
   alongside eboot.bin.
4. Refresh your native homebrew mounts as required.
5. Launch OutRunners from the PS5 dashboard.
6. Allow the first-time game data installation to finish.

Do not extract the contents of orunners.zip manually.

## Folder structure

    /data/homebrew/PPSA98538/
        eboot.bin
        orunners.zip
        sce_module/
            libc.prx
        sce_sys/
            icon0.png
            param.json
            pic0.dds
            pic1.dds

The orunners.zip file is supplied by the user.

The installer expects the MAME World ROM set and
validates the ROM contents during extraction.

## First launch

OutRunners-PS5 automatically prepares its game data
on the console.

The installation screen displays four stages:

1. Checking ROMs
2. Processing game data
3. Saving game data
4. Verifying game data

Do not close the application during installation.

Once preparation is complete, OutRunners starts
automatically.

No separate PC extraction utility is required.

The generated asset cache is approximately 31 MB.
Additional temporary storage is required during
installation.

Subsequent launches use the verified cached data.

Keep your original ROM archive for future installations
or cache reconstruction.

## Upgrading

Close the application before replacing application files.

Back up your existing settings, game data and saved
progress before upgrading.

Do not unnecessarily delete existing user data.

Refresh the native application mounts if required.

## Application storage

The following paths are visible to the running application:

| Path | Purpose |
|---|---|
| /app0/orunners.zip | User-supplied ROM archive |
| /download0/outrunners.assets | Verified game data cache |
| /download0/outrunners-settings.bin | Persistent settings |
| /download0/outrunners-rom-install.log | Installation diagnostics |
| /download0/outrunners.log | Application diagnostics |
| /download0/orunners-install/ | Temporary installation workspace |

These are application-visible mount paths.

Their physical locations depend on the installed
homebrew environment.

## Troubleshooting

### ROM SET REQUIRED

Check that orunners.zip is beside eboot.bin
in the application folder.

### INVALID ZIP FILE

Check that the archive is non-empty, is within
the installer's 128 MiB input limit, and contains
the correct MAME World ROM set.

### ROM INSTALLATION FAILED

Read the on-screen error details.

Check the ROM set and, where available,
the outrunners-rom-install.log diagnostic file.

### STORAGE ERROR or CACHE WRITE FAILED

Check available writable storage and the
homebrew environment's storage configuration.

### CACHE VERIFICATION FAILED

The generated game data did not pass verification.
Check the ROM set and installation diagnostics.

### Application does not appear on the dashboard

Check the complete PPSA98538 directory structure
and refresh your native homebrew mounting environment.

### Reporting problems

Include:

- OutRunners-PS5 release version
- PS5 firmware version
- Native homebrew loader and version
- Exact error message
- Relevant diagnostic log excerpts
- Steps required to reproduce the problem

Do not upload ROM archives, generated game asset packs,
personal saves or credentials with issue reports.

## Compatibility

v1.0.0 has been tested on physical PS5 hardware.
Compatibility with every firmware and native homebrew
loader version has not been established.
