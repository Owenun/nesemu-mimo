#include "nesemu/machine.hpp"

#include <cstdio>
#include <fstream>
#include <string>

using nesemu::u8;
using nesemu::u16;

static nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Hook by wrapping: poll $2001 via debug each instruction is too slow.
// Instead run frames and after each frame print mask, and once detect $2001 change.

int main(int argc, char** argv) {
  if (argc < 2) {
    return 2;
  }
  auto machine = nesemu::Machine::load_rom(read_file(argv[1]));
  if (!machine) {
    return 1;
  }
  u8 last_mask = machine->ppu().reg_peek(0x2001);
  u8 last_ctrl = machine->ppu().reg_peek(0x2000);
  int irq_hits = 0;
  for (int f = 0; f < 300; ++f) {
    for (int s = 0; s < 100000; ++s) {
      const u16 pc = machine->cpu().pc();
      if (pc == 0xFFF0) {
        ++irq_hits;
        if (irq_hits <= 3) {
          std::printf("IRQ vector hit frame %d P=%02X\n", f, machine->cpu().p());
        }
      }
      machine->step_instruction();
      const u8 mask = machine->ppu().reg_peek(0x2001);
      const u8 ctrl = machine->ppu().reg_peek(0x2000);
      if (mask != last_mask) {
        std::printf("frame~%d $2001 %02X -> %02X  PC=%04X\n", f, last_mask, mask,
                    machine->cpu().pc());
        last_mask = mask;
      }
      if (ctrl != last_ctrl) {
        std::printf("frame~%d $2000 %02X -> %02X  PC=%04X\n", f, last_ctrl, ctrl,
                    machine->cpu().pc());
        last_ctrl = ctrl;
      }
    }
  }
  std::printf("irq_hits=%d final mask=%02X\n", irq_hits, last_mask);
  return 0;
}
