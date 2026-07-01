import re
import urllib.request

UA = "PIPBONG/0.7.5"
for slug in ["omens", "fragments", "currency"]:
    url = f"https://poe.ninja/poe2/economy/runesofaldur/{slug}"
    html = urllib.request.urlopen(
        urllib.request.Request(url, headers={"User-Agent": UA}), timeout=20
    ).read().decode("utf-8", "replace")
    print(f"\n=== {slug} ===")
    for m in sorted(set(re.findall(r"[A-Za-z]{3,30}Omen[a-zA-Z]{0,20}", html))):
        print(" ", m)
    for m in sorted(set(re.findall(r'"overviewKey":"([^"]+)"', html))):
        print(" overviewKey:", m)
    for m in sorted(set(re.findall(r'"category":"([^"]+)"', html))):
        print(" category:", m)
    for m in sorted(set(re.findall(r'"apiType":"([^"]+)"', html))):
        print(" apiType:", m)
    idx = html.find("Omen of Chance")
    if idx >= 0:
        print(" snippet:", html[max(0, idx - 200) : idx + 200].replace("\n", " ")[:400])
