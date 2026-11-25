#!/usr/bin/env python3

import argparse
import ftplib
import hashlib
import json
import os
import re
import sys
import uuid

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent

HOST = os.environ.get(
    "PS5_HOST", ""
)

PORT = int(os.environ.get(
    "FTP_PORT", "2121"
))

USER = os.environ.get(
    "PS5_FTP_USER", "anonymous"
)

PASSWORD = os.environ.get(
    "PS5_FTP_PASSWORD", "codex"
)

TIMEOUT = 20


def local_hash(path):
    digest = hashlib.sha256()

    with path.open("rb") as file:
        for chunk in iter(
            lambda: file.read(65536), b""
        ):
            digest.update(chunk)

    return digest.hexdigest()


def remote_hash(ftp, path):
    digest = hashlib.sha256()

    try:
        ftp.retrbinary(
            f"RETR {path}",
            digest.update,
            blocksize=65536
        )
    except ftplib.error_perm as error:
        if str(error).startswith("550"):
            return None

        raise

    return digest.hexdigest()


def ensure_directory(ftp, directory):
    parts = directory.strip("/").split("/")

    current = ""

    for part in parts:
        current += "/" + part

        try:
            ftp.cwd(current)

        except ftplib.error_perm as error:
            if not str(error).startswith("550"):
                raise

            ftp.mkd(current)
            ftp.cwd(current)


def upload_verified(
    ftp, local, remote, expected_hash
):
    temporary = (
        remote +
        ".upload-" +
        uuid.uuid4().hex[:12]
    )

    try:
        with local.open("rb") as file:
            ftp.storbinary(
                f"STOR {temporary}",
                file,
                blocksize=65536
            )

        if remote_hash(
            ftp, temporary
        ) != expected_hash:
            raise RuntimeError(
                f"Uploaded checksum mismatch: {remote}"
            )

        # Replace the destination only after the
        # temporary upload has been verified.
        #
        # Never delete the existing destination
        # as a fallback if renaming fails.

        ftp.rename(temporary, remote)

        if remote_hash(
            ftp, remote
        ) != expected_hash:
            raise RuntimeError(
                f"Final checksum mismatch: {remote}"
            )

    except Exception:
        try:
            ftp.delete(temporary)
        except ftplib.all_errors:
            pass

        raise


def get_application():
    metadata = json.loads(
        (ROOT / "sce_sys/param.json").read_text()
    )

    title = metadata["titleId"]

    if not re.fullmatch(r"PPSA[0-9]{5}", title):
        raise RuntimeError(
            f"Invalid title ID: {title}"
        )

    dist = ROOT / "dist" / title

    required = [
        "eboot.bin",
        "sce_module/libc.prx",
        "sce_sys/param.json",
        "sce_sys/icon0.png"
    ]

    for name in required:
        path = dist / name

        if not path.is_file():
            raise RuntimeError(
                f"Missing build artifact: {path}"
            )

    packaged = json.loads(
        (dist / "sce_sys/param.json").read_text()
    )

    if packaged["titleId"] != title:
        raise RuntimeError(
            "Packaged title ID does not match project"
        )

    if packaged["contentId"] != metadata["contentId"]:
        raise RuntimeError(
            "Packaged content ID does not match project"
        )

    return title, dist


def collect_files(dist):
    files = []

    # Restrict automatic deployment to executable,
    # launcher assets, runtime modules and res/.
    #
    # ROMs, saves, configuration and other data
    # directories are deliberately excluded.

    allowed_directories = {
        "sce_sys",
        "sce_module",
        "res"
    }

    for path in sorted(dist.rglob("*")):
        relative = path.relative_to(dist)

        if path.is_symlink():
            raise RuntimeError(
                f"Unexpected distribution symlink: {relative}"
            )

        if not path.is_file():
            continue

        parts = relative.parts

        allowed = (
            relative.as_posix() == "eboot.bin"
            or parts[0] in allowed_directories
        )

        if not allowed:
            continue

        if not all(
            re.fullmatch(
                r"[A-Za-z0-9_. -]+", part
            )
            for part in parts
        ):
            raise RuntimeError(
                f"Unsupported filename: {relative}"
            )

        files.append((relative.as_posix(), path))

    # Upload the executable last.
    files.sort(key=lambda item: (
        item[0] == "eboot.bin",
        item[0]
    ))

    return files



def configure_raw_transfer(ftp, title, dist):
    """
    ftpsrv may convert signed SELF files to ELF
    during downloads. Detect the current mode
    before toggling it.
    """

    remote = f"/data/homebrew/{title}/eboot.bin"

    with (dist / "eboot.bin").open("rb") as file:
        expected_magic = file.read(4)

    def probe():
        header = bytearray()

        def consume(chunk):
            if len(header) < 4:
                header.extend(
                    chunk[:4-len(header)]
                )

        try:
            ftp.retrbinary(
                f"RETR {remote}",
                consume,
                blocksize=65536
            )

        except ftplib.error_perm as error:
            if str(error).startswith("550"):
                return None

            raise

        return bytes(header)

    magic = probe()

    if magic == expected_magic:
        print("FTP raw binary mode: VERIFIED")
        return

    if magic is None:
        # New installation: use the documented
        # ftpsrv default session behaviour.
        # The subsequent upload checksum check
        # remains mandatory.
        response = ftp.sendcmd("SELF")

        print("SELF command:", response)
        print(
            "Raw mode requested for new installation."
        )

        return

    if magic != b"\x7fELF":
        raise RuntimeError(
            "Unexpected remote executable format: "
            + magic.hex()
        )

    print("FTP SELF-to-ELF conversion detected.")

    response = ftp.sendcmd("SELF")

    print("SELF command:", response)

    if probe() != expected_magic:
        raise RuntimeError(
            "Unable to verify raw SELF retrieval."
        )

    print("FTP raw binary mode: VERIFIED")


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--check",
        action="store_true",
        help="Compare remote files without uploading"
    )

    args = parser.parse_args()
    if not HOST.strip():
        parser.error("Set PS5_HOST explicitly before deploying")

    title, dist = get_application()
    files = collect_files(dist)

    remote_root = f"/data/homebrew/{title}"

    print()
    print("========================================")
    print(" PS5 FTP INCREMENTAL DEPLOYMENT")
    print("========================================")
    print(f"Host:        {HOST}:{PORT}")
    print(f"Title:       {title}")
    print(f"Destination: {remote_root}/")
    print(f"Mode:        {'CHECK ONLY' if args.check else 'UPLOAD'}")
    print()

    unchanged = 0
    changed = 0
    uploaded = 0
    transferred = 0

    with ftplib.FTP() as ftp:
        ftp.connect(
            HOST, PORT, timeout=TIMEOUT
        )

        ftp.login(USER, PASSWORD)
        ftp.set_pasv(True)

        configure_raw_transfer(ftp, title, dist)

        print("FTP connection: OK")
        print()

        if not args.check:
            ensure_directory(ftp, remote_root)

        for relative, local in files:
            remote = remote_root + "/" + relative

            expected = local_hash(local)
            existing = remote_hash(ftp, remote)

            if existing == expected:
                print(f"UNCHANGED  {relative}")

                unchanged += 1
                continue

            changed += 1

            status = (
                "NEW" if existing is None
                else "CHANGED"
            )

            if args.check:
                print(f"{status:10} {relative}")
                continue

            print(
                f"UPLOADING  {relative}",
                flush=True
            )

            ensure_directory(
                ftp, remote.rsplit("/", 1)[0]
            )

            upload_verified(
                ftp,
                local,
                remote,
                expected
            )

            uploaded += 1
            transferred += local.stat().st_size

            print(f"VERIFIED   {relative}")

    print()
    print("========================================")
    print(" FTP COMPARISON COMPLETE"
          if args.check
          else " FTP DEPLOYMENT COMPLETE")
    print("========================================")
    print(f"Unchanged files: {unchanged}")
    print(f"Changed files:   {changed}")
    print(f"Uploaded files:  {uploaded}")
    print(f"Bytes uploaded:  {transferred:,}")

    if args.check:
        print("Remote modifications: NONE")
    else:
        print("Uploaded files: CHECKSUM VERIFIED")
        print("Remote files deleted: NONE")
        print("ROMs and saves: UNTOUCHED")

        if changed:
            print()
            print("Application ready for testing.")

        else:
            print()
            print("Application already up to date.")


if __name__ == "__main__":
    try:
        main()
    except (Exception, KeyboardInterrupt) as error:
        print(
            f"\nFTP DEPLOYMENT FAILED: {error}",
            file=sys.stderr
        )

        print(
            "Local application build is retained.",
            file=sys.stderr
        )

        sys.exit(1)
