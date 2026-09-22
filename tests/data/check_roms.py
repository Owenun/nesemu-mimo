import os
base = r"C:\Users\xzc\Downloads"
for n in os.listdir(base):
    p = os.path.join(base, n)
    if not os.path.isfile(p):
        continue
    if any(k in n for k in ("口袋", "俄罗", "超级", "魂斗", "F1", "f1")):
        d = open(p, "rb").read(16)
        print(f"{n!r} size={os.path.getsize(p)} magic={d[:4]!r}")
