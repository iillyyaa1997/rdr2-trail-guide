#!/usr/bin/env python3
"""nat.py NAME_OR_HASH ... : print matching natives (namespace::signature) from src/sdk/natives.gen.h"""
import re, sys
ns = None
rows = []
for line in open("src/sdk/natives.gen.h", encoding="utf-8"):
    m = re.match(r"namespace (\w+) \{", line)
    if m: ns = m.group(1); continue
    if "inline" in line and "invoke<" in line:
        sig = line.split("{")[0].replace("inline ", "").replace("template <typename... Va> ", "").strip()
        h = re.search(r"\((0x[0-9A-F]+)ULL", line).group(1)
        rows.append((ns, sig, h))
for q in sys.argv[1:]:
    qq = q.upper()
    hits = [r for r in rows if qq in r[1].split("(")[0].upper() or qq == r[2].upper()]
    for r in hits[:6]: print(f"{r[0]}::{r[1]}  {r[2]}")
    if not hits: print(f"-- {q}: none")
