"""List economy items missing from EconomyNameKoData.inc."""
from __future__ import annotations

import re
from pathlib import Path

import importlib.util

spec = importlib.util.spec_from_file_location(
    "fetch", Path(__file__).parent / "fetch-poe2db-ko-names.py"
)
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)

inc = Path(__file__).resolve().parents[1] / "src/core/poeninja/EconomyNameKoData.inc"
text = inc.read_text(encoding="utf-8")
mapped = set(re.findall(r'QStringLiteral\("([^"]+)"\), QStringLiteral', text))
items = fetch.load_items()
unmapped = [(c, i, n) for c, i, n in items if f"{c}:{i}" not in mapped]
print(f"unmapped {len(unmapped)}")
for c, i, n in unmapped:
    print(f"{c}:{i} | {n}")
