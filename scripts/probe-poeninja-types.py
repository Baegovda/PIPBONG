import json
import re
import urllib.parse
import urllib.request

BASE = "https://poe.ninja/poe2/api"
VER = "0757-20260701-40472"
LEAGUE = "Runes of Aldur"
UA = "PIPBONG/0.7.5"

SLUGS = [
    ("fragments", "Fragments"),
    ("abyssal-bones", None),
    ("uncut-gems", "UncutGems"),
    ("lineage-support-gems", "LineageSupportGems"),
    ("essences", "Essences"),
    ("soul-cores", "SoulCores"),
    ("idols", "Idols"),
    ("runes", "Runes"),
    ("omens", None),
    ("expedition", "Expedition"),
    ("liquid-emotions", None),
    ("breach-catalyst", "Breach"),
    ("verisium", "Verisium"),
]


def fetch_overview(type_name: str) -> int:
    url = (
        f"{BASE}/economy/exchange/{VER}/overview?"
        f"league={urllib.parse.quote(LEAGUE)}&type={type_name}"
    )
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    data = json.load(urllib.request.urlopen(req, timeout=20))
    return len(data.get("lines", []))


def scrape_slug(slug: str) -> list[str]:
    url = f"https://poe.ninja/poe2/economy/runesofaldur/{slug}"
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    html = urllib.request.urlopen(req, timeout=20).read().decode("utf-8", "replace")
    hits = set(re.findall(r'"type":"([A-Za-z]+)"', html))
    hits |= set(re.findall(r"type=([A-Za-z]+)", html))
    hits |= set(re.findall(r"overviewType[^A-Za-z]+([A-Za-z]+)", html))
    return sorted(hits)


OMEN_TYPES = [
    "Omen",
    "Omens",
    "OmenItem",
    "OmenItems",
    "Azmeri",
    "Voodoo",
    "AzmeriOmen",
    "VoodooOmen",
    "Sign",
    "Signs",
    "Charm",
    "Charms",
]


def probe_omens() -> None:
    print("\n=== omens brute ===")
    for t in OMEN_TYPES:
        try:
            n = fetch_overview(t)
            if n:
                print(f"  {t}: {n} lines")
        except Exception:
            pass


def scrape_page_api(slug: str) -> None:
    url = f"https://poe.ninja/poe2/economy/runesofaldur/{slug}"
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    html = urllib.request.urlopen(req, timeout=20).read().decode("utf-8", "replace")
    print(f"\n=== scrape {slug} html {len(html)} ===")
    for token in sorted(set(re.findall(r"/poe2/api/[a-zA-Z0-9_./?=&%-]+", html))):
        print(" ", token[:120])
    for token in sorted(set(re.findall(r'"type":"([A-Za-z]+)"', html))):
        print("  json type:", token)


if __name__ == "__main__":
    probe_omens()
    scrape_page_api("omens")
    for slug, known in SLUGS:
        print(f"\n=== {slug} (known={known}) ===")
        if known:
            try:
                print(f"  exchange type {known}: {fetch_overview(known)} lines")
            except Exception as ex:
                print(f"  exchange type {known}: ERR {ex}")
        for hit in scrape_slug(slug):
            if hit == known:
                continue
            try:
                n = fetch_overview(hit)
                if n:
                    print(f"  scrape candidate {hit}: {n} lines")
            except Exception:
                pass
