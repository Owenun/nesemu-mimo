#include "nesemu/machine.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

using nesemu::u8;
using nesemu::u16;

static nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: nesemu_debug <rom> [frames]\n");
    return 2;
  }
  int frames = 180;
  if (argc >= 3) {
    frames = std::atoi(argv[2]);
  }
  const bool autoplay = (argc >= 4 && std::strcmp(argv[3], "autoplay") == 0);
  auto machine = nesemu::Machine::load_rom(read_file(argv[1]));
  if (!machine) {
    return 1;
  }
  auto& cpu = machine->cpu();
  auto& ppu = machine->ppu();
  u8 prev32 = 0xFF;
  for (int i = 0; i < frames; ++i) {
    if (autoplay) {
      u8 pad = 0;
      if (i >= 70 && i < 76) {
        pad = 0x08;
      }
      machine->set_controller_state(0, pad);
    }
    bool prev_irq = false;
    for (int step = 0; step < 40000; ++step) {
      machine->step_instruction();
      const bool irq = machine->mapper().irq_line();
      if (irq && !prev_irq) {
        std::printf("IRQ FIRE F=%d sl=%u dot=%u\n", i, ppu.scanline(), ppu.dot());
      }
      prev_irq = irq;
      if (ppu.scanline() == 241 && ppu.dot() < 4) {
        break;
      }
    }
    const u8 z32 = machine->bus().peek(0x32);
    const u8 z6b = machine->bus().peek(0x6B);
    const u8 z6a = machine->bus().peek(0x6A);
    const u8 z3b = machine->bus().peek(0x3B);
    const u8 z67 = machine->bus().peek(0x67);
    const bool stuck = (z32 == prev32);
    if (stuck || i < 8 || i % 10 == 0 || i == frames - 1) {
      std::printf("F%03d PC=%04X z32=%02X z6B=%02X z6A=%02X z3B=%02X z67=%02X nmi=%llu%s\n", i,
                  cpu.pc(), z32, z6b, z6a, z3b, z67,
                  static_cast<unsigned long long>(cpu.nmi_count()), stuck ? " STUCK" : "");
    }
    if (i % 25 == 0) {
      char mdb[256];
      machine->mapper().dump_debug(mdb, sizeof(mdb));
      std::printf("  MAP %s ctrl=%02X\n", mdb, ppu.reg_peek(0x2000));
    }
    prev32 = z32;
  }
  std::printf("PC=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X CYC=%llu\n", cpu.pc(), cpu.a(), cpu.x(),
              cpu.y(), cpu.sp(), cpu.p(), static_cast<unsigned long long>(cpu.cycles()));
  std::printf("STACK:");
  for (int i = 0; i < 32; ++i) {
    std::printf(" %02X", machine->bus().peek(static_cast<u16>(0x0100 + ((cpu.sp() + 1 + i) & 0xFF))));
  }
  std::printf("\n");
  std::printf("8000bytes:");
  for (int i = 0; i < 16; ++i) {
    std::printf(" %02X", machine->bus().peek(static_cast<u16>(0x8000 + i)));
  }
  std::printf("\n974Fbytes:");
  for (int i = 0; i < 16; ++i) {
    std::printf(" %02X", machine->bus().peek(static_cast<u16>(0x974F + i)));
  }
  std::printf("\n");
  std::printf("PCbytes:");
  for (int i = 0; i < 16; ++i) {
    std::printf(" %02X", machine->bus().peek(static_cast<u16>(cpu.pc() + i)));
  }
  std::printf("\n");
  std::printf("PPU sl=%u dot=%u frame=%llu ctrl=%02X mask=%02X status=%02X\n", ppu.scanline(),
              ppu.dot(), static_cast<unsigned long long>(ppu.frame()), ppu.reg_peek(0x2000),
              ppu.reg_peek(0x2001), ppu.reg_peek(0x2002));
  std::printf("APU=$%02X\n", machine->apu().read_status_peek());
  std::printf("pal:");
  for (int i = 0; i < 32; ++i) {
    std::printf(" %02X", ppu.palette_peek(static_cast<u8>(i)));
  }
  std::printf("\n");
  std::printf("OAM[0..31]:");
  for (int i = 0; i < 32; ++i) {
    std::printf(" %02X", ppu.oam()[static_cast<std::size_t>(i)]);
  }
  std::printf("\n");
  std::printf("RAM $20-$3F:");
  for (int i = 0x20; i < 0x40; ++i) {
    std::printf(" %02X", machine->bus().peek(static_cast<u16>(i)));
  }
  std::printf("\nNMIcount=%llu zp6B=%02X zp32=%02X zp0512=%02X\n", (unsigned long long)cpu.nmi_count(),
              machine->bus().peek(0x6B), machine->bus().peek(0x32), machine->bus().peek(0x0512));
  char mdb[256];
  machine->mapper().dump_debug(mdb, sizeof(mdb));
  std::printf("MAP %s\n", mdb);
  std::printf("t=%04X v=%04X x=%u w=%d\n", ppu.reg_peek(0x2000), 0, 0, 0);
  std::printf("NMIvec=%04X IRQvec=%04X\n",
              static_cast<unsigned>(machine->bus().peek(0xFFFA) |
                                    (machine->bus().peek(0xFFFB) << 8)),
              static_cast<unsigned>(machine->bus().peek(0xFFFE) |
                                    (machine->bus().peek(0xFFFF) << 8)));
  std::printf("RAM $00-$1F:");
  for (int i = 0; i < 32; ++i) {
    std::printf(" %02X", machine->bus().peek(static_cast<u16>(i)));
  }
  std::printf("\nRAM $0770-$078F:");
  for (int i = 0x770; i < 0x790; ++i) {
    std::printf(" %02X", machine->bus().peek(static_cast<u16>(i)));
  }
  std::printf("\n");
  // Nametable sample
  std::printf("NT[0..31]:");
  for (int i = 0; i < 32; ++i) {
    std::printf(" %02X", ppu.vram_peek(static_cast<u16>(0x2000 + i)));
  }
  std::printf("\n");
  // Bottom nametable rows (y=192..239 → tile rows 24-29)
  for (int row = 0; row < 30; ++row) {
    std::printf("NTrow%02d:", row);
    for (int col = 0; col < 32; ++col) {
      std::printf(" %02X", ppu.vram_peek(static_cast<u16>(0x2000 + row * 32 + col)));
    }
    std::printf("\n");
    if ((row % 4) == 3) {
      std::printf("ATblk%02d:", row / 4);
      for (int col = 0; col < 8; ++col) {
        std::printf(" %02X", ppu.vram_peek(static_cast<u16>(0x23C0 + (row / 4) * 8 + col)));
      }
      std::printf("\n");
    }
  }
  // CHR sample: tile 0 and a few tiles from BG pattern
  std::printf("CHR[0x0000..0x0020]:");
  for (int i = 0; i < 32; ++i) {
    std::printf(" %02X", machine->mapper().ppu_peek(static_cast<u16>(i)));
  }
  std::printf("\nCHR[0x0100..0x0120]:");
  for (int i = 0; i < 32; ++i) {
    std::printf(" %02X", machine->mapper().ppu_peek(static_cast<u16>(0x100 + i)));
  }
  std::printf("\n");
  // Ground tiles 0x65-0x6A at $0000 and $1000 pattern tables
  for (u16 tile : {0x65, 0x66, 0x67, 0x68, 0x69, 0x6A}) {
    std::printf("T%02X@$0000:", tile);
    for (int i = 0; i < 16; ++i) {
      std::printf(" %02X", machine->mapper().ppu_peek(static_cast<u16>(tile * 16 + i)));
    }
    std::printf("  T%02X@$1000:", tile);
    for (int i = 0; i < 16; ++i) {
      std::printf(" %02X", machine->mapper().ppu_peek(static_cast<u16>(0x1000 + tile * 16 + i)));
    }
    std::printf("\n");
  }
  // Scroll snapshot
  std::printf("scroll t=? v=? (ctrl=%02X)\n", ppu.reg_peek(0x2000));
  // Screen stats
  const auto& fb = ppu.framebuffer();
  int hist[64] = {};
  for (int i = 0; i < 256 * 240; ++i) {
    hist[fb.pixels[static_cast<std::size_t>(i)] & 63]++;
  }
  std::printf("screen color hist:");
  for (int i = 0; i < 64; ++i) {
    if (hist[i]) {
      std::printf(" %02X:%d", i, hist[i]);
    }
  }
  std::printf("\n");
  return 0;
}
