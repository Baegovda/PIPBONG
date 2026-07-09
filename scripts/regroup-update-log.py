#!/usr/bin/env python3
"""Regroup UpdateLog/update_log.md by patch decade (e.g. v0.8.60–0.8.69).

Preserves existing Korean bullets; merges sections under one header per block.
"""

from __future__ import annotations

import re
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
UPDATE_LOG = REPO_ROOT / "UpdateLog" / "update_log.md"

HEADER_RE = re.compile(
    r"^## v(?P<ver>\d+\.\d+\.\d+)(?:–\d+\.\d+\.\d+)? \((?P<date>\d{4}/\d{2}/\d{2})(?: – \d{4}/\d{2}/\d{2})?\)\s*$",
    re.MULTILINE,
)
SECTION_RE = re.compile(r"^\*\*(추가|변경|수정|제거)\*\*\s*$", re.MULTILINE)


def parse_version(ver: str) -> tuple[int, int, int]:
    a, b, c = ver.split(".")
    return int(a), int(b), int(c)


def decade_key(ver: str) -> tuple[int, int, int]:
    major, minor, patch = parse_version(ver)
    return major, minor, patch // 10


def decade_label(major: int, minor: int, decade: int) -> str:
    low = decade * 10
    high = low + 9
    if low == high == 0 and decade == 0:
        # single-digit patch range 0–9
        return f"v{major}.{minor}.0–{major}.{minor}.9"
    return f"v{major}.{minor}.{low}–{major}.{minor}.{high}"


def parse_entries(text: str) -> list[dict]:
    parts = re.split(r"^## v", text, flags=re.MULTILINE)
    intro = parts[0]
    entries: list[dict] = []
    for part in parts[1:]:
        m = re.match(
            r"(?P<ver>\d+\.\d+\.\d+) \((?P<date>\d{4}/\d{2}/\d{2})\)\s*\n(?P<body>.*)",
            part,
            re.DOTALL,
        )
        if not m:
            continue
        ver = m.group("ver")
        body = m.group("body").strip()
        sections: dict[str, list[str]] = defaultdict(list)
        current: str | None = None
        for line in body.splitlines():
            sm = SECTION_RE.match(line.strip())
            if sm:
                current = sm.group(1)
                continue
            if line.strip().startswith("- ") or line.strip().startswith("_("):
                if current:
                    sections[current].append(line.rstrip())
                else:
                    sections.setdefault("_note", []).append(line.rstrip())
        entries.append(
            {
                "ver": ver,
                "date": m.group("date"),
                "sections": dict(sections),
            }
        )
    return intro, entries


def merge_entries(entries: list[dict]) -> list[dict]:
    groups: dict[tuple[int, int, int], list[dict]] = defaultdict(list)
    for e in entries:
        groups[decade_key(e["ver"])].append(e)

    merged: list[dict] = []
    for key in sorted(groups.keys(), reverse=True):
        major, minor, decade = key
        items = sorted(groups[key], key=lambda x: parse_version(x["ver"]), reverse=True)
        dates = [i["date"] for i in items]
        date_range = dates[0] if dates[0] == dates[-1] else f"{dates[-1]} – {dates[0]}"
        sections: dict[str, list[str]] = defaultdict(list)
        notes: list[str] = []
        for item in items:
            for note in item["sections"].get("_note", []):
                if note not in notes:
                    notes.append(note)
            for sec in ("추가", "변경", "수정", "제거"):
                for bullet in item["sections"].get(sec, []):
                    sections[sec].append(bullet)
        merged.append(
            {
                "label": decade_label(major, minor, decade),
                "date_range": date_range,
                "notes": notes,
                "sections": dict(sections),
            }
        )
    return merged


def render(intro: str, merged: list[dict]) -> str:
    # Keep only the standard intro through first ---
    intro_lines = intro.strip().splitlines()
    out = intro_lines + ["", "---", ""]
    for block in merged:
        out.append(f"## {block['label']} ({block['date_range']})")
        out.append("")
        for note in block.get("notes", []):
            out.append(note)
            out.append("")
        for sec in ("추가", "변경", "수정", "제거"):
            bullets = block["sections"].get(sec)
            if not bullets:
                continue
            out.append(f"**{sec}**")
            out.append("")
            out.extend(bullets)
            out.append("")
    return "\n".join(out).rstrip() + "\n"


def main() -> None:
    text = UPDATE_LOG.read_text(encoding="utf-8")
    intro, entries = parse_entries(text)
    merged = merge_entries(entries)
    UPDATE_LOG.write_text(render(intro, merged), encoding="utf-8")
    print(f"Regrouped {len(entries)} versions into {len(merged)} blocks -> {UPDATE_LOG}")


if __name__ == "__main__":
    main()
