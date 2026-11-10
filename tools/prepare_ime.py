#!/usr/bin/env python3
"""Prepare the pinned, offline Google Pinyin decoder and 32-bit dictionary."""

import base64
import hashlib
import io
from pathlib import Path
import re
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
VERSION = "0.1.2"
SOURCE_URL = (
    "https://storage.googleapis.com/google-code-archive-downloads/v2/"
    f"code.google.com/libgooglepinyin/libgooglepinyin-{VERSION}.tar.bz2"
)
SOURCE_SHA256 = "1a339ae45721a60b9fadd15e43c34b9bb27af3bb999c00ed0d88b4084cfd0637"
DICT_URL = (
    "https://android.googlesource.com/platform/packages/inputmethods/PinyinIME/"
    "+/android-4.0.4_r2.1/res/raw/dict_pinyin.dat?format=TEXT"
)
DICT_SHA256 = "6bf0bbde4e3134cce38d08524a9f4dc1af40435243c8e36b38eb68d1e14462b2"
SOURCE = ROOT / ".build" / "vendor" / f"libgooglepinyin-{VERSION}"
DEST = ROOT / "ime-data"


def fetch(url):
    print(f"Downloading {url}", flush=True)
    with urllib.request.urlopen(url, timeout=90) as response:
        return response.read()


def checked(data, expected):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError("Downloaded Pinyin resource failed SHA-256 validation")
    return data


def main():
    marker = SOURCE / ".prepared"
    if not marker.is_file():
        source = checked(fetch(SOURCE_URL), SOURCE_SHA256)
        copyrights = set()
        with tarfile.open(fileobj=io.BytesIO(source), mode="r:bz2") as archive:
            prefix = f"libgooglepinyin-{VERSION}/"
            for member in archive.getmembers():
                if not member.isfile() or not member.name.startswith(prefix):
                    continue
                relative = Path(member.name[len(prefix):])
                if relative.is_absolute() or ".." in relative.parts:
                    raise ValueError("Unsafe archive member")
                stream = archive.extractfile(member)
                if stream is None:
                    raise ValueError("Unreadable archive member")
                content = stream.read()
                target = SOURCE / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(content)
                if target.suffix in (".h", ".cpp"):
                    copyrights.update(re.findall(rb"Copyright[^\r\n]*", content))
        SOURCE.mkdir(parents=True, exist_ok=True)
        (SOURCE / "COPYRIGHTS.txt").write_bytes(b"\n".join(sorted(copyrights)) + b"\n")
        marker.write_text(SOURCE_SHA256 + "\n", encoding="ascii")

    # Upstream assumes a root search can never exhaust its finite milestone pool.
    # Repeated initials can exhaust it: return no match instead of aborting st.
    matrix = SOURCE / "src" / "matrixsearch.cpp"
    code = matrix.read_text(encoding="utf-8")
    original = "    assert(0 != handles[0]);"
    guarded = "    if (0 == handles[0]) return 0;  // rc_st-sdl: bounded search pool"
    if original in code:
        if code.count(original) != 1:
            raise ValueError("Unexpected upstream root-search assertion")
        matrix.write_text(code.replace(original, guarded), encoding="utf-8")
    elif guarded not in code:
        raise ValueError("Cannot apply bounded-search compatibility fix")
    spelling = SOURCE / "src" / "spellingtrie.cpp"
    code = spelling.read_text(encoding="utf-8")
    original_delete = "    delete spl_ym_ids_;"
    fixed_delete = "    delete [] spl_ym_ids_;  // rc_st-sdl: array allocation"
    if original_delete in code:
        spelling.write_text(code.replace(original_delete, fixed_delete), encoding="utf-8")
    elif fixed_delete not in code:
        raise ValueError("Cannot apply spelling-array lifetime fix")
    lifetime_fixes = (
        (
            "matrixsearch.cpp",
            "    ch_pos++;\n  }\n\n  // Get spelling ids and starting positions.",
            "    ch_pos++;\n"
            "    get_spl_start_id();\n"
            "    if (spl_id_num_ > max_hzs_len_) {\n"
            "      reset_search(--ch_pos, false, false, false);\n"
            "      get_spl_start_id();\n"
            "      break;\n"
            "    }\n  }\n\n  // Get spelling ids and starting positions.",
        ),
        (
            "dicttrie.cpp",
            "  if (NULL != nodes_ge1_)\n    free(nodes_ge1_);\n  nodes_ge1_ = NULL;\n\n  if (free_dict_list)",
            "  if (NULL != lma_idx_buf_)\n    free(lma_idx_buf_);\n  lma_idx_buf_ = NULL;\n\n  if (free_dict_list)",
        ),
        (
            "userdict.cpp",
            "  free(ids_);\n#ifdef ___PREDICT_ENABLED___",
            "  free(ids_);\n#ifdef ___SYNC_ENABLED___\n  free(syncs_);\n#endif\n#ifdef ___PREDICT_ENABLED___",
        ),
        (
            "spellingtrie.cpp",
            "  root_ = new SpellingNode();",
            "  if (root_) { free_son_trie(root_); delete root_; }\n"
            "  delete dumb_node_;\n  delete splitter_node_;\n  root_ = new SpellingNode();",
        ),
    )
    for filename, old, new in lifetime_fixes:
        target = SOURCE / "src" / filename
        code = target.read_text(encoding="utf-8")
        if new in code:
            continue
        if code.count(old) != 1:
            raise ValueError(f"Cannot apply decoder lifetime fix: {filename}")
        target.write_text(code.replace(old, new), encoding="utf-8")
    prepared = SOURCE_SHA256 + "\nbounded-search-v1\narray-delete-v1\nlifetime-v1\nsyllable-limit-v1\n"
    if marker.read_text(encoding="ascii") != prepared:
        marker.write_text(prepared, encoding="ascii")
    elif marker.stat().st_mtime < Path(__file__).stat().st_mtime:
        marker.touch()

    DEST.mkdir(parents=True, exist_ok=True)
    dictionary = DEST / "dict_pinyin.dat"
    if not dictionary.is_file() or hashlib.sha256(dictionary.read_bytes()).hexdigest() != DICT_SHA256:
        dictionary.write_bytes(checked(base64.b64decode(fetch(DICT_URL)), DICT_SHA256))
    elif dictionary.stat().st_mtime < marker.stat().st_mtime:
        dictionary.touch()
    license_path = DEST / "LICENSE-2.0.txt"
    if not license_path.is_file():
        license_path.write_bytes(fetch("https://www.apache.org/licenses/LICENSE-2.0.txt"))
    (DEST / "NOTICE.txt").write_text(
        "Google Pinyin decoder libgooglepinyin 0.1.2 and Android PinyinIME dictionary.\n"
        "Copyright (C) 2009 The Android Open Source Project.\n"
        "Licensed under the Apache License, Version 2.0; see LICENSE-2.0.txt.\n"
        "Local decoder modification: exhausted root-search pools return no match\n"
        "instead of triggering an assertion. Dictionary data is unmodified.\n"
        "Local decoder modification: match spelling-array delete[] to new[]\n"
        "when reopening the decoder.\n"
        "Local decoder modification: release dictionary index/sync buffers and\n"
        "old spelling trees when closing or reopening the decoder.\n"
        "Local decoder modification: enforce the configured syllable limit\n"
        "during each search step before fixed search pools can overflow.\n"
        f"Decoder source: {SOURCE_URL}\n"
        f"Dictionary source: {DICT_URL}\n"
        "Upstream source file notices:\n"
        + (SOURCE / "COPYRIGHTS.txt").read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    print(f"Pinyin resources ready: {SOURCE}, {DEST}")


if __name__ == "__main__":
    main()
