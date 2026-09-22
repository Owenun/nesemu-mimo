#include "nesemu/machine.hpp"

#include <cstdio>
#include <fstream>
#include <string>

using nesemu::u8;
using nesemu::u16;

namespace {

nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

// Blargg-style: result at $6000, $6001..$6003 = $DE $B0 $61 signature when done.
// Also supports blargg mmc3_irq_tests style: zero-page $F8 == 1 means pass
// (after the test reaches report_final_result and prints).
int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: nesemu_rom_test <rom> [max_frames=300]\n");
    return 2;
  }
  int max_frames = 300;
  if (argc >= 3) {
    max_frames = std::atoi(argv[2]);
  }
  auto rom = read_file(argv[1]);
  auto machine = nesemu::Machine::load_rom(rom);
  if (!machine) {
    std::printf("load failed\n");
    return 1;
  }

  for (int i = 0; i < max_frames; ++i) {
    machine->run_until_frame();
    const auto& bus = machine->bus();
    // Classic Blargg $6000 protocol
    const u8 s0 = bus.peek(0x6001);
    const u8 s1 = bus.peek(0x6002);
    const u8 s2 = bus.peek(0x6003);
    if (s0 == 0xDE && s1 == 0xB0 && s2 == 0x61) {
      const u8 code = bus.peek(0x6000);
      if (code == 0x80) {
        std::printf("PASS (frame %d)\n", i);
        return 0;
      }
      std::printf("FAIL code=%u (frame %d)\n", code, i);
      std::string msg;
      for (int a = 0x6004; a < 0x7F00; ++a) {
        const char c = static_cast<char>(bus.peek(static_cast<std::uint16_t>(a)));
        if (c == 0) {
          break;
        }
        msg.push_back(c);
      }
      std::printf("msg: %s\n", msg.c_str());
      return 1;
    }
    // mmc3_irq_tests / validation.asm: result at $F8 (1=pass, else fail code).
    // Detect completion when PC parks in the forever loop after reporting.
    const u8 result = bus.peek(0xF8);
    const u16 pc = machine->cpu().pc();
    if (result == 1) {
      // Confirm it's not just a transient value: require PC near report/forever
      // or just accept after several frames with result==1.
      static int stable = 0;
      ++stable;
      if (stable > 5) {
        std::printf("PASS via $F8=1 (frame %d)\n", i);
        return 0;
      }
    } else if (result > 1 && result < 0x80) {
      static int stable_fail = 0;
      ++stable_fail;
      if (stable_fail > 5) {
        std::printf("FAIL code=%u at $F8 (frame %d) PC=%04X\n", result, i, pc);
        return 1;
      }
    }
    if (machine->cpu().jammed()) {
      std::printf("CPU jammed at frame %d\n", i);
      return 1;
    }
  }
  std::printf("timeout after %d frames PC=%04X $F8=%02X $6000=%02X\n", max_frames,
              machine->cpu().pc(), machine->bus().peek(0xF8), machine->bus().peek(0x6000));
  return 1;
}
