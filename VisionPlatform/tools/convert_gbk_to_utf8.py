# -*- coding: utf-8 -*-
"""Convert GBK-encoded source files to UTF-8 and update string helpers."""
from __future__ import print_function

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(ROOT)

SKIP_DIRS = {".git", "obj", "bin", "third_party"}
EXTS = {".cpp", ".h", ".hpp", ".inc", ".rc", ".ui"}


def should_skip(path):
    parts = path.replace("\\", "/").split("/")
    return any(p in SKIP_DIRS for p in parts)


def detect_encoding(data):
    if data.startswith(b"\xef\xbb\xbf"):
        return "utf-8-sig", data[3:]
    try:
        data.decode("utf-8")
        return "utf-8", data
    except UnicodeDecodeError:
        pass
    try:
        return "gbk", data
    except UnicodeDecodeError:
        return None, data


def convert_file(path):
    with open(path, "rb") as f:
        data = f.read()
    enc, payload = detect_encoding(data)
    if enc in ("utf-8", "utf-8-sig"):
        return False, "utf-8"
    if enc != "gbk":
        return False, "unknown"
    text = payload.decode("gbk")
    tmp = path + ".utf8tmp"
    with open(tmp, "wb") as f:
        f.write(text.encode("utf-8"))
    os.replace(tmp, path)
    return True, "gbk->utf-8"


def patch_text_helpers():
    qtconv = os.path.join(REPO, "VisionPlatform", "VisionUi", "main", "QtConv.h")
    with open(qtconv, "rb") as f:
        text = f.read().decode("utf-8")
    text = text.replace(
        "/** @brief 本地代码页 std::string → QString（中文 Windows 为 GBK） */",
        "/** @brief UTF-8 std::string → QString */",
    )
    text = text.replace(
        "return QString::fromLocal8Bit(s.data(), static_cast<int>(s.size()));",
        "return QString::fromUtf8(s.data(), static_cast<int>(s.size()));",
    )
    text = text.replace(
        "/** @brief QString → 本地代码页 std::string */",
        "/** @brief QString → UTF-8 std::string */",
    )
    text = text.replace(
        "const QByteArray b = s.toLocal8Bit();",
        "const QByteArray b = s.toUtf8();",
    )
    with open(qtconv, "wb") as f:
        f.write(text.encode("utf-8"))

    uitext = os.path.join(REPO, "VisionPlatform", "VisionWidgets", "theme", "UiText.h")
    with open(uitext, "rb") as f:
        text = f.read().decode("utf-8")
    text = text.replace(
        "return QString::fromLocal8Bit(Vision::Translate::tr(key));",
        "return QString::fromUtf8(Vision::Translate::tr(key));",
    )
    with open(uitext, "wb") as f:
        f.write(text.encode("utf-8"))

    props = os.path.join(REPO, "VisionPlatform", "vision.props")
    with open(props, "rb") as f:
        text = f.read().decode("utf-8")
    if "/utf-8" not in text:
        text = text.replace(
            "      <MinimalRebuild>false</MinimalRebuild>\n",
            "      <MinimalRebuild>false</MinimalRebuild>\n"
            "      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>\n",
        )
        text = text.replace(
            "      <!-- Align with AOI/StdTool: source/runtime use system ANSI (Chinese Windows=GBK), no /utf-8 -->",
            "      <!-- Source and execution charset: UTF-8 (/utf-8). UI strings use QString::fromUtf8. -->",
        )
    with open(props, "wb") as f:
        f.write(text.encode("utf-8"))


def main():
    converted = 0
    skipped = 0
    for dirpath, dirnames, filenames in os.walk(REPO):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        if should_skip(dirpath):
            continue
        for fn in filenames:
            ext = os.path.splitext(fn)[1].lower()
            if ext not in EXTS:
                continue
            path = os.path.join(dirpath, fn)
            try:
                did, msg = convert_file(path)
                if did:
                    converted += 1
                else:
                    skipped += 1
            except Exception as exc:
                print("ERROR %s: %s" % (path, exc))
                return 1
    patch_text_helpers()
    print("converted %d files, skipped %d already utf-8/unknown" % (converted, skipped))
    return 0


if __name__ == "__main__":
    sys.exit(main())
