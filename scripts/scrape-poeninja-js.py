import re
import urllib.request

UA = "PIPBONG/0.7.5"
url = "https://poe.ninja/poe2/economy/runesofaldur/omens"
html = urllib.request.urlopen(
    urllib.request.Request(url, headers={"User-Agent": UA}), timeout=20
).read().decode("utf-8", "replace")

scripts = re.findall(r'src="([^"]+\.js)"', html)
print("scripts", len(scripts))
for src in scripts[:8]:
    if not src.startswith("http"):
        src = "https://poe.ninja" + src
    print("fetch", src)
    js = urllib.request.urlopen(
        urllib.request.Request(src, headers={"User-Agent": UA}), timeout=30
    ).read().decode("utf-8", "replace")
    for needle in ["Omens", "Omen", "AbyssalBones", "Delirium", "LiquidEmotion", "overviewType", "exchange"]:
        if needle in js:
            print("  hit", needle, "count", js.count(needle))
    for m in re.finditer(r"type:\"([A-Za-z]+)\"", js):
        t = m.group(1)
        if "omen" in t.lower() or "abyss" in t.lower() or "delirium" in t.lower() or "liquid" in t.lower():
            print("  type:", t)
