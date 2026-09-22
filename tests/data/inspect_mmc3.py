d = open(r"C:\Users\xzc\XiaomiMiMoProjects\nes-test-roms\mmc3_irq_tests\1.Clocking.nes", "rb").read()
prg = d[16 : 16 + 16384]


def vec(o):
    return prg[o] | (prg[o + 1] << 8)


rst = vec(0x3FFC)
irq = vec(0x3FFE)
nmi = vec(0x3FFA)
print("RST=%04X NMI=%04X IRQ=%04X" % (rst, nmi, irq))
off = rst - 0xC000
print("rst:", prg[off : off + 96].hex(" "))
ioff = irq - 0xC000
if 0 <= ioff < len(prg):
    print("irq:", prg[ioff : ioff + 48].hex(" "))
# search for DE B0 61 signature constants
for i in range(len(prg) - 3):
    if prg[i] == 0xDE and prg[i + 1] == 0xB0 and prg[i + 2] == 0x61:
        print("found DEB061 at prg+%04X cpu=%04X" % (i, 0xC000 + i))
