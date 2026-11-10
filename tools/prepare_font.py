#!/usr/bin/env python3
"""Fetch an unmodified GNU Unifont BMP bitmap and its upstream notices."""

import gzip
import io
from pathlib import Path
import re
import tarfile
import urllib.request


VERSION = "17.0.03"
BASE = f"https://unifoundry.com/pub/unifont/unifont-{VERSION}"
DEST = Path(__file__).resolve().parent.parent / "fonts"


def download(url):
    print(f"Downloading {url}", flush=True)
    with urllib.request.urlopen(url, timeout=90) as response:
        return response.read()


def main():
    outputs = ("unifont.hex", "OFL-1.1.txt", "COPYING", "README.upstream", "FONT-NOTICE.txt")
    if all((DEST / name).is_file() for name in outputs):
        print(f"Font and notices already exist in {DEST}")
        return

    font = gzip.decompress(download(f"{BASE}/font-builds/unifont-{VERSION}.hex.gz"))
    glyphs = {}
    for line in font.splitlines():
        if not re.fullmatch(rb"[0-9A-Fa-f]{4}:(?:[0-9A-Fa-f]{32}|[0-9A-Fa-f]{64})", line):
            raise ValueError("Unexpected Unifont BMP .hex record")
        glyphs[int(line[:4], 16)] = line
    if not all(codepoint in glyphs for codepoint in map(ord, "中文测试繁體你好")):
        raise ValueError("Downloaded font lacks required Chinese glyphs")

    archive = download(f"{BASE}/unifont-{VERSION}.tar.gz")
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as source:
        notices = {}
        for name in ("OFL-1.1.txt", "COPYING"):
            member = source.extractfile(f"unifont-{VERSION}/{name}")
            if member is None:
                raise ValueError(f"Missing upstream notice: {name}")
            notices[name] = member.read()
        member = source.extractfile(f"unifont-{VERSION}/README")
        if member is None:
            raise ValueError("Missing upstream README")
        notices["README.upstream"] = member.read()

    # Fetch and validate everything before publishing usable resource files.
    DEST.mkdir(parents=True, exist_ok=True)
    for name, data in notices.items():
        (DEST / name).write_bytes(data)
    (DEST / "FONT-NOTICE.txt").write_text(
        f"GNU Unifont {VERSION}, unmodified BMP .hex bitmap.\n"
        f"Source: {BASE}/font-builds/unifont-{VERSION}.hex.gz\n"
        "Distributed under the upstream SIL Open Font License 1.1 option.\n"
        "See OFL-1.1.txt for the license, COPYING for upstream licensing\n"
        "information, and README.upstream for the authors' notices.\n",
        encoding="utf-8",
    )
    (DEST / "unifont.hex").write_bytes(font)
    print(f"Prepared {len(glyphs)} glyphs in {DEST}; copy the whole fonts/ directory.")


if __name__ == "__main__":
    main()
