#!/usr/bin/env python3
"""Generate UpdateLog/update_log.md draft from AGENTS.md §11.

Strips trailing developer references only. Review and polish Korean before release.
"""

from __future__ import annotations

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
AGENTS = REPO_ROOT / "AGENTS.md"
OUTPUT = REPO_ROOT / "UpdateLog" / "update_log.md"

SECTION_KO = {
    "Added": "추가",
    "Changed": "변경",
    "Fixed": "수정",
    "Removed": "제거",
}

# Trailing (File.cpp, ClassName) or (JSON `key`) dev citations — not user parens like (≥2)
TRAILING_DEV = re.compile(
    r"\s*\((?:[^)]*(?:\.cpp|\.h|::|JSON|QSettings|§|`)[^)]*)\)\s*\.?\s*$"
)
TRAILING_MULTI_FILES = re.compile(
    r"\s*\([A-Za-z][A-Za-z0-9_:]*(?:,\s*[A-Za-z][A-Za-z0-9_:]*)+\)\s*\.?\s*$"
)


def strip_dev_refs(line: str) -> str:
    s = line.strip()
    if not s.startswith("- "):
        return ""
    s = s[2:].strip()
    prev = None
    while s != prev:
        prev = s
        s = TRAILING_DEV.sub("", s).strip()
        s = TRAILING_MULTI_FILES.sub("", s).strip()
    s = re.sub(r"\s{2,}", " ", s).strip(" .")
    return f"- {s}" if s else ""


def parse_changelog(text: str) -> list[tuple[str, str, dict[str, list[str]]]]:
    start = text.find("## [Unreleased]")
    if start < 0:
        raise SystemExit("AGENTS.md: missing ## [Unreleased]")
    body = text[start:]
    version_re = re.compile(
        r"^## \[(?P<ver>\d+\.\d+\.\d+)\] - (?P<date>\d{4}-\d{2}-\d{2})\s*\n(.*?)(?=^## \[|\Z)",
        re.MULTILINE | re.DOTALL,
    )
    entries: list[tuple[str, str, dict[str, list[str]]]] = []
    for m in version_re.finditer(body):
        if m.group("ver") == "Unreleased":
            continue
        sections: dict[str, list[str]] = {}
        block = m.group(3)
        sec_re = re.compile(
            r"^### (Added|Changed|Fixed|Removed)\s*\n(.*?)(?=^### |\Z)",
            re.MULTILINE | re.DOTALL,
        )
        for sm in sec_re.finditer(block):
            name = sm.group(1)
            bullets = []
            for raw in sm.group(2).splitlines():
                cleaned = strip_dev_refs(raw)
                if cleaned:
                    bullets.append(cleaned)
            if bullets:
                sections[name] = bullets
        if sections:
            entries.append((m.group("ver"), m.group("date"), sections))
    return entries


def format_date(iso: str) -> str:
    y, m, d = iso.split("-")
    return f"{y}/{m}/{d}"


def render(entries: list[tuple[str, str, dict[str, list[str]]]]) -> str:
    lines = [
        "# PIPBONG 업데이트 로그",
        "",
        "버전별 변경 사항을 한글로 정리한 문서입니다. 최신 릴리즈는 "
        "[GitHub Releases](https://github.com/Baegovda/PIPBONG/releases/latest)에서 받을 수 있습니다.",
        "",
        "개발용 상세 기록(영문)은 [`AGENTS.md`](../AGENTS.md) §11을 참고하세요.",
        "",
        "---",
        "",
    ]
    for ver, date, sections in entries:
        lines.append(f"## v{ver} ({format_date(date)})")
        lines.append("")
        for en_name in ("Added", "Changed", "Fixed", "Removed"):
            bullets = sections.get(en_name)
            if not bullets:
                continue
            lines.append(f"**{SECTION_KO[en_name]}**")
            lines.append("")
            lines.extend(bullets)
            lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def main() -> None:
    text = AGENTS.read_text(encoding="utf-8")
    entries = parse_changelog(text)
    # User-facing log: 0.8.x and notable 0.7.x+
    entries = [e for e in entries if e[0] >= "0.7.0"]
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(render(entries), encoding="utf-8")
    print(f"Wrote {OUTPUT} ({len(entries)} versions)")


if __name__ == "__main__":
    main()
