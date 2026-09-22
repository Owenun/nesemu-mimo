for name in ["smb", "contra", "f1"]:
    d = open(f"tests/data/{name}.nes", "rb").read()
    prg = d[4] * 16384
    prg_rom = d[16 : 16 + prg]

    def vec(o):
        return prg_rom[o] | (prg_rom[o + 1] << 8)

    if prg == 16384:
        nmi, rst, irq = vec(0x3FFA), vec(0x3FFC), vec(0x3FFE)
    else:
        nmi, rst, irq = vec(prg - 6), vec(prg - 4), vec(prg - 2)
    print(f"{name} prg={prg} NMI=${nmi:04X} RST=${rst:04X} IRQ=${irq:04X}")

    if prg == 16384:
        off = rst - 0xC000
    else:
        off = rst - 0x8000
    print("  rst:", prg_rom[off : off + 64].hex())
    nmi_off = (nmi - 0x8000) if prg > 16384 else (nmi - 0xC000)
    if 0 <= nmi_off < prg:
        print("  nmi:", prg_rom[nmi_off : nmi_off + 32].hex())
