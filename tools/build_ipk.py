#!/usr/bin/env python3
"""Build a reproducible RetroFW IPK from the prepared D8/D88 application."""

import argparse
import gzip
import hashlib
import io
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parent.parent
PACKAGE = "st-sdl"
ARCH = "mipsel"
APP_PATH = Path("home/apps/st-sdl")
MENU_PATH = Path("home/.gmenunx/sections/applications/st.lnk")
# A fixed FAT-compatible timestamp keeps archive bytes reproducible.
EPOCH = max(315532800, int(os.environ.get("SOURCE_DATE_EPOCH", "315532800")))


def tool(name):
    sdk = Path("/opt/retrofw/bin") / name
    return str(sdk) if sdk.is_file() else shutil.which(name)


def copy_file(source, destination, executable=False):
    if not source.is_file():
        raise FileNotFoundError(f"Missing package resource: {source}; run make package-d8")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    destination.chmod(0o755 if executable else 0o644)


def check_mips_elf(path):
    header = path.read_bytes()[:64]
    if len(header) < 52 or header[:6] != b"\x7fELF\x01\x01" or int.from_bytes(header[18:20], "little") != 8:
        raise ValueError(f"Expected a 32-bit little-endian MIPS ELF: {path}")


def tar_gz(directory):
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.USTAR_FORMAT) as archive:
        for path in sorted(directory.rglob("*")):
            name = "./" + path.relative_to(directory).as_posix()
            info = archive.gettarinfo(str(path), arcname=name)
            info.uid = info.gid = 0
            info.uname = info.gname = "root"
            info.mtime = EPOCH
            if info.isfile():
                with path.open("rb") as stream:
                    archive.addfile(info, stream)
            elif info.isdir():
                archive.addfile(info)
            else:
                raise ValueError(f"Unexpected non-regular package entry: {path}")
    compressed = io.BytesIO()
    with gzip.GzipFile(fileobj=compressed, mode="wb", filename="", mtime=EPOCH, compresslevel=9) as stream:
        stream.write(raw.getvalue())
    return compressed.getvalue()


def ar_package(members):
    result = bytearray(b"!<arch>\n")
    for name, content in members:
        header = f"{name + '/':<16}{EPOCH:<12}{0:<6}{0:<6}{'100644':<8}{len(content):<10}`\n".encode("ascii")
        if len(header) != 60:
            raise ValueError("Invalid ar member header")
        result.extend(header)
        result.extend(content)
        if len(content) % 2:
            result.extend(b"\n")
    return bytes(result)


def build(args):
    source = args.build_dir.resolve()
    check_mips_elf(source / "st")
    check_mips_elf(source / "libst-preload.so")
    tic = tool("tic")
    if not tic:
        raise RuntimeError("tic is required to bundle the st/st-256color terminfo entries")
    strip = tool("mipsel-linux-strip") if not args.no_strip else None
    (ROOT / ".build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ipk-", dir=ROOT / ".build") as temporary:
        workspace = Path(temporary)
        data, control = workspace / "data", workspace / "control"
        app = data / APP_PATH
        app.mkdir(parents=True)
        control.mkdir()
        for name in ("st", "libst-preload.so", "README.zh-CN.md"):
            copy_file(source / name, app / name, name in ("st", "libst-preload.so"))
        for name in ("st.png", "st.info", "LICENSE"):
            copy_file(ROOT / name, app / name)
        for name in ("unifont.hex", "OFL-1.1.txt", "COPYING", "README.upstream", "FONT-NOTICE.txt", "README"):
            copy_file(source / "fonts" / name, app / "fonts" / name)
        for name in ("dict_pinyin.dat", "LICENSE-2.0.txt", "NOTICE.txt", "README"):
            copy_file(source / "ime-data" / name, app / "ime-data" / name)
        # user.dat is deliberately outside the package's file ownership.
        if strip:
            subprocess.run([strip, "--strip-unneeded", str(app / "st"), str(app / "libst-preload.so")], check=True)
        check_mips_elf(app / "st")
        check_mips_elf(app / "libst-preload.so")

        menu = data / MENU_PATH
        menu.parent.mkdir(parents=True)
        menu.write_text(
            "title=st\n"
            "description=UTF-8 terminal with Shift Pinyin input\n"
            "exec=/home/apps/st-sdl/st\n"
            "icon=/home/apps/st-sdl/st.png\n",
            encoding="ascii",
        )
        terminfo = data / "home/.terminfo"
        terminfo.mkdir(parents=True)
        subprocess.run([tic, "-o", str(terminfo), str(ROOT / "st.info")], check=True)
        for name in ("st", "st-256color"):
            if not (terminfo / "s" / name).is_file():
                raise ValueError(f"tic did not generate the expected terminfo entry: {name}")

        files = sorted(path for path in data.rglob("*") if path.is_file())
        for directory in (path for path in data.rglob("*") if path.is_dir()):
            directory.chmod(0o755)
        for path in files:
            if path not in (app / "st", app / "libst-preload.so"):
                path.chmod(0o644)
        installed_size = sum(path.stat().st_size for path in files)
        (control / "control").write_text(
            f"Package: {PACKAGE}\nVersion: {args.version}\n"
            "Description: UTF-8 SDL terminal with offline Pinyin for RetroFW D8/D88\n"
            " Shift toggles Chinese/English; supports phrase candidates, user learning,\n"
            " shell and Vim input. Includes bitmap fonts, dictionaries and GMenuNX link.\n"
            "Section: apps\nPriority: optional\nMaintainer: D8/D88 Community\n"
            f"Architecture: {ARCH}\nInstalled-Size: {installed_size}\n",
            encoding="ascii",
        )
        (control / "conffiles").write_text("/" + MENU_PATH.as_posix() + "\n", encoding="ascii")
        (control / "md5sums").write_text(
            "".join(f"{hashlib.md5(path.read_bytes()).hexdigest()}  {path.relative_to(data).as_posix()}\n" for path in files),
            encoding="ascii",
        )
        for path in control.iterdir():
            path.chmod(0o644)
        archive = ar_package((
            ("debian-binary", b"2.0\n"),
            ("control.tar.gz", tar_gz(control)),
            ("data.tar.gz", tar_gz(data)),
        ))

    args.output_dir.mkdir(parents=True, exist_ok=True)
    output = args.output_dir / f"{PACKAGE}_{args.version}_{ARCH}.ipk"
    pending = output.with_suffix(".ipk.tmp")
    pending.write_bytes(archive)
    pending.replace(output)
    checksum = hashlib.sha256(archive).hexdigest()
    output.with_suffix(".ipk.sha256").write_text(f"{checksum}  {output.name}\n", encoding="ascii")
    print(f"IPK: {output.resolve()}")
    print(f"Size: {len(archive)} bytes; installed files: {len(files)}; payload: {installed_size} bytes")
    print(f"SHA256: {checksum}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default="0.4.0-1", help="IPK package version")
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".build/d88")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist")
    parser.add_argument("--no-strip", action="store_true", help="Keep binary debug information")
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9][A-Za-z0-9.+~\-]*", args.version):
        parser.error("Invalid package version")
    build(args)


if __name__ == "__main__":
    main()
