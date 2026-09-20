#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generator pentru web_assets.h.

Comprima portal.html si auth.html cu gzip si le scrie ca tablouri de octeti in
web_assets.h, de unde le include sketch-ul. Firmware-ul le trimite ca atare, cu
antetul `Content-Encoding: gzip`, iar browserul le decomprima singur - pagina
ajunge la utilizator identica bit cu bit.

    RULEAZA ASTA DE FIECARE DATA CAND MODIFICI portal.html SAU auth.html,
    INAINTE DE A COMPILA. Altfel flash-ezi versiunea veche a interfetei.

        python build_web.py            regenereaza web_assets.h
        python build_web.py --check    doar verifica daca headerul e la zi
                                       (cod de iesire 1 daca nu e)
"""
import gzip
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "web_assets.h")

# (fisier sursa, numele simbolului in C++)
ASSETS = [
    ("portal.html", "PORTAL_HTML_GZ"),
    ("auth.html", "AUTH_SHELL_GZ"),
]


def emit_array(name, blob):
    lines = [f"const uint8_t {name}[] PROGMEM = {{"]
    for i in range(0, len(blob), 16):
        chunk = blob[i:i + 16]
        lines.append("  " + " ".join(f"0x{b:02x}," for b in chunk))
    lines.append("};")
    lines.append(f"const uint32_t {name}_LEN = {len(blob)};")
    return "\n".join(lines)


def build():
    parts = [
        "// FISIER GENERAT - nu edita manual.",
        "// Sursa: " + ", ".join(a for a, _ in ASSETS),
        "// Regenereaza cu:  python build_web.py",
        "#pragma once",
        "#include <pgmspace.h>",
        "#include <stdint.h>",
        "",
    ]
    report = []
    for fname, sym in ASSETS:
        raw = open(os.path.join(HERE, fname), "rb").read()
        # mtime=0 so an unchanged page produces an identical header and the
        # sketch does not get recompiled for no reason.
        blob = gzip.compress(raw, compresslevel=9, mtime=0)
        parts.append(f"// {fname}: {len(raw)} -> {len(blob)} bytes")
        parts.append(f"#define {sym}_SRC_SHA \"{hashlib.sha256(raw).hexdigest()[:16]}\"")
        parts.append(emit_array(sym, blob))
        parts.append("")
        report.append((fname, len(raw), len(blob)))
    return "\n".join(parts) + "\n", report


def main():
    text, report = build()
    check = "--check" in sys.argv

    current = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else None
    if check:
        if current == text:
            print("web_assets.h este la zi.")
            return 0
        print("web_assets.h NU este la zi - ruleaza: python build_web.py")
        return 1

    if current == text:
        print("web_assets.h era deja la zi, nimic de facut.")
    else:
        open(OUT, "w", encoding="utf-8", newline="\n").write(text)
        print("web_assets.h regenerat.")

    total_raw = sum(r for _, r, _ in report)
    total_gz = sum(g for _, _, g in report)
    for fname, raw, gz in report:
        print(f"  {fname:12} {raw:>8,} -> {gz:>7,} ({gz / raw * 100:.1f}%)")
    print(f"  {'TOTAL':12} {total_raw:>8,} -> {total_gz:>7,} "
          f"(economie {total_raw - total_gz:,} octeti in flash)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
