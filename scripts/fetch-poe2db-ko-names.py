"""Fetch PoE2DB Korean names for poe.ninja economy items and emit C++ map source."""
from __future__ import annotations

import json
import re
import time
import unicodedata
import urllib.parse
import urllib.request
from pathlib import Path

POE2_API = "https://poe.ninja/poe2/api"
POE2DB = "https://poe2db.tw/kr"
UA = "PIPBONG/0.7.7"
VERSION = "0757-20260701-40472"
LEAGUE = "Runes of Aldur"

CATEGORIES = [
    ("currency", "Currency"),
    ("fragments", "Fragments"),
    ("abyssal-bones", "Abyss"),
    ("uncut-gems", "UncutGems"),
    ("lineage-support-gems", "LineageSupportGems"),
    ("essences", "Essences"),
    ("soul-cores", "SoulCores"),
    ("idols", "Idols"),
    ("runes", "Runes"),
    ("omens", "Ritual"),
    ("expedition", "Expedition"),
    ("liquid-emotions", "Delirium"),
    ("breach-catalyst", "Breach"),
    ("verisium", "Verisium"),
]


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.load(resp)


def fold_accents(value: str) -> str:
    normalized = unicodedata.normalize("NFKD", value)
    return "".join(ch for ch in normalized if not unicodedata.combining(ch))


def synthetic_korean_name(english_name: str) -> str | None:
    m = re.fullmatch(r"Uncut Skill Gem \(Level (\d+)\)", english_name)
    if m:
        return f"미가공 스킬 젬 (레벨 {m.group(1)})"
    m = re.fullmatch(r"Uncut Spirit Gem \(Level (\d+)\)", english_name)
    if m:
        return f"미가공 정신력 젬 (레벨 {m.group(1)})"
    m = re.fullmatch(r"Uncut Support Gem \(Level (\d+)\)", english_name)
    if m:
        return f"미가공 보조 젬 (레벨 {m.group(1)})"
    m = re.fullmatch(r"Thaumaturgic Flux \(Level (\d+)\)", english_name)
    if m:
        return f"마석학 유동체 ({m.group(1)}레벨)"
    return None


def slug_candidates(name: str) -> list[str]:
    paths: list[str] = []

    def add(candidate: str) -> None:
        candidate = candidate.strip("_")
        if candidate and candidate not in paths:
            paths.append(candidate)

    add(re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_"))
    add(re.sub(r"[^A-Za-z0-9-]+", "_", name).strip("_"))
    if ":" in name:
        left, right = name.split(":", 1)
        left_slug = re.sub(r"[^A-Za-z0-9-]+", "_", left).strip("_")
        right_slug = re.sub(r"[^A-Za-z0-9]+", "_", right).strip("_")
        if left_slug and right_slug:
            add(f"{left_slug}%3A_{right_slug}")
    m = re.fullmatch(r"(.+?) \(Level (\d+)\)", name)
    if m:
        base = re.sub(r"[^A-Za-z0-9-]+", "_", m.group(1)).strip("_")
        level = m.group(2)
        add(f"{base}_%28Level_{level}%29")
        add(f"{base}_(Level_{level})")
    return paths


def wiki_paths(english_name: str) -> list[str]:
    base = english_name.strip()
    variants = [base, fold_accents(base)]
    variants.append(re.sub(r"['’]", "", base))
    variants.append(re.sub(r"'s\b", "s", base))
    variants.append(re.sub(r"'s\b", "s", fold_accents(base)))
    variants.append(base.replace("'", ""))
    paths: list[str] = []
    for name in variants:
        for slug in slug_candidates(name):
            if slug not in paths:
                paths.append(slug)
    return paths


def extract_korean_name(html: str, english_name: str) -> str | None:
    # PoE2DB item pages list paired BaseType rows: English then Korean.
    pairs = re.findall(
        r"BaseType\s*\|\s*([^\n|]+)",
        html,
    )
    cleaned = [p.strip() for p in pairs if p.strip()]
    for i, value in enumerate(cleaned):
        if value == english_name and i + 1 < len(cleaned):
            ko = cleaned[i + 1]
            if ko != english_name and re.search(r"[\uac00-\ud7a3]", ko):
                return ko
    if english_name in html:
        for value in cleaned:
            if value != english_name and re.search(r"[\uac00-\ud7a3]", value):
                return value
    # Title line before " - PoE2DB"
    m = re.search(r"<title>([^<]+)\s*-\s*PoE2DB", html, re.I)
    if m:
        title = m.group(1).strip()
        if title != english_name and re.search(r"[\uac00-\ud7a3]", title):
            return title.split(" Currency")[0].strip()
    return None


MANUAL_KOREAN_BY_COMPOSITE: dict[str, str] = {
    "currency:annul": "소멸의 오브",
    "lineage-support-gems:oisins-oath": "오이신의 서약",
    "lineage-support-gems:mórrigans-insight": "모리건의 통찰",
    "runes:aldurs-legacy": "알두르의 유산",
    "runes:betrayal-of-aldur": "알두르의 배신",
    "runes:breath-of-aldur": "알두르의 숨결",
    "runes:ward-rune": "수호 룬",
    "runes:ire-of-aldur": "알두르의 노여움",
    "runes:passion-of-aldur": "알두르의 열정",
    "runes:rebirth-rune": "부활 룬",
    "runes:stone-rune": "돌 룬",
    "runes:vision-rune": "환영 룬",
    "expedition:aldurs-saga": "알두르의 영웅담",
}


def resolve_korean_name(
    english_name: str, cache: dict[str, str | None], composite_id: str = ""
) -> str | None:
    if composite_id and composite_id in MANUAL_KOREAN_BY_COMPOSITE:
        ko = MANUAL_KOREAN_BY_COMPOSITE[composite_id]
        cache[english_name] = ko
        return ko
    synthetic = synthetic_korean_name(english_name)
    if synthetic:
        cache[english_name] = synthetic
        return synthetic
    return fetch_korean_name(english_name, cache)


def fetch_korean_name(english_name: str, cache: dict[str, str | None]) -> str | None:
    if english_name in cache:
        return cache[english_name]
    for slug in wiki_paths(english_name):
        if "%" in slug:
            url = f"{POE2DB}/{slug}"
        else:
            url = f"{POE2DB}/{urllib.parse.quote(slug)}"
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=20) as resp:
                html = resp.read().decode("utf-8", "replace")
            ko = extract_korean_name(html, english_name)
            if ko:
                cache[english_name] = ko
                return ko
        except Exception:
            continue
        time.sleep(0.05)
    cache[english_name] = None
    return None


def load_items() -> list[tuple[str, str, str]]:
    items: list[tuple[str, str, str]] = []
    for category_id, api_type in CATEGORIES:
        url = (
            f"{POE2_API}/economy/exchange/{VERSION}/overview?"
            f"league={urllib.parse.quote(LEAGUE)}&type={api_type}"
        )
        data = fetch_json(url)
        for entry in data.get("items", []):
            items.append((category_id, entry["id"], entry["name"]))
    return items


def cpp_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def main() -> None:
    items = load_items()
    cache: dict[str, str | None] = {}
    mappings: dict[str, str] = {}

    # Re-use existing currency map keys from hand-maintained file when possible.
    for i, (category_id, api_id, english_name) in enumerate(items):
        ko = resolve_korean_name(english_name, cache, f"{category_id}:{api_id}")
        if ko:
            composite = f"{category_id}:{api_id}"
            mappings[composite] = ko
        if (i + 1) % 25 == 0:
            print(f"progress {i + 1}/{len(items)} mapped={len(mappings)}")
            time.sleep(0.2)

    out_cpp = Path(__file__).resolve().parents[1] / "src/core/poeninja/EconomyNameKoData.inc"
    lines = [
        "// Auto-generated by scripts/fetch-poe2db-ko-names.py — do not hand-edit.",
        "// Source: https://poe2db.tw/kr/",
    ]
    for composite in sorted(mappings):
        ko = cpp_escape(mappings[composite])
        key = cpp_escape(composite)
        lines.append(
            '        {QStringLiteral("' + key + '"), QStringLiteral("' + ko + '")},'
        )
    out_cpp.write_text("\n".join(lines) + "\n", encoding="utf-8")

    summary = {
        "total_items": len(items),
        "mapped": len(mappings),
        "unmapped": len(items) - len(mappings),
    }
    print(json.dumps(summary, indent=2))
    unmapped = [
        f"{c}:{i} {n}"
        for c, i, n in items
        if f"{c}:{i}" not in mappings
    ]
    if unmapped:
        print("unmapped sample:", unmapped[:20])


if __name__ == "__main__":
    main()
