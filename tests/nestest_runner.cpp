#include "nesemu/machine.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct LogLine {
  unsigned pc = 0, a = 0, x = 0, y = 0, p = 0, sp = 0, cyc = 0;
};

bool parse_log_line(const std::string& line, LogLine& out) {
  // Typical: C000  4C F5 C5  JMP $C5F5   A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 21 CYC:7
  const std::size_t apos = line.find("A:");
  const std::size_t xpos = line.find("X:");
  const std::size_t ypos = line.find("Y:");
  const std::size_t ppos = line.find("P:");
  const std::size_t sppos = line.find("SP:");
  const std::size_t cpos = line.find("CYC:");
  if (apos == std::string::npos || cpos == std::string::npos) {
    return false;
  }
  out.pc = static_cast<unsigned>(std::stoul(line.substr(0, 4), nullptr, 16));
  out.a = static_cast<unsigned>(std::stoul(line.substr(apos + 2, 2), nullptr, 16));
  out.x = static_cast<unsigned>(std::stoul(line.substr(xpos + 2, 2), nullptr, 16));
  out.y = static_cast<unsigned>(std::stoul(line.substr(ypos + 2, 2), nullptr, 16));
  out.p = static_cast<unsigned>(std::stoul(line.substr(ppos + 2, 2), nullptr, 16));
  out.sp = static_cast<unsigned>(std::stoul(line.substr(sppos + 3, 2), nullptr, 16));
  out.cyc = static_cast<unsigned>(std::stoul(line.substr(cpos + 4)));
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::printf("usage: nesemu_nestest <nestest.nes> <nestest.log> [max_ops=8991]\n");
    return 2;
  }
  const std::string rom_path = argv[1];
  const std::string log_path = argv[2];
  int max_ops = 8991;
  if (argc >= 4) {
    max_ops = std::atoi(argv[3]);
  }

  auto rom = read_file(rom_path);
  auto machine = nesemu::Machine::load_rom(rom);
  if (!machine) {
    std::printf("failed to load ROM\n");
    return 1;
  }

  // nestest automated mode starts at $C000
  machine->cpu().set_pc(0xC000);
  machine->cpu().set_p(0x24);
  machine->cpu().set_sp(0xFD);
  machine->cpu().set_a(0);
  machine->cpu().set_x(0);
  machine->cpu().set_y(0);
  // Reset already consumed cycles; nestest log CYC starts at 7 after reset.
  // Our reset also takes ~7 bus cycles. Align cycles to 7.
  machine->cpu().set_cycles(7);

  std::ifstream log(log_path);
  if (!log) {
    std::printf("failed to open log\n");
    return 1;
  }

  std::string line;
  int compared = 0;
  int failed = 0;
  while (compared < max_ops && std::getline(log, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    LogLine want;
    if (!parse_log_line(line, want)) {
      continue;
    }

    auto& cpu = machine->cpu();
    const unsigned pc = cpu.pc();
    const unsigned a = cpu.a();
    const unsigned x = cpu.x();
    const unsigned y = cpu.y();
    const unsigned p = cpu.p();
    const unsigned sp = cpu.sp();
    const unsigned cyc = static_cast<unsigned>(cpu.cycles());

    if (pc != want.pc || a != want.a || x != want.x || y != want.y || p != want.p ||
        sp != want.sp || cyc != want.cyc) {
      std::printf("mismatch at op #%d\n  got  PC=%04X A=%02X X=%02X Y=%02X P=%02X SP=%02X CYC=%u\n"
                  "  want PC=%04X A=%02X X=%02X Y=%02X P=%02X SP=%02X CYC=%u\n  line: %s\n",
                  compared, pc, a, x, y, p, sp, cyc, want.pc, want.a, want.x, want.y, want.p,
                  want.sp, want.cyc, line.c_str());
      ++failed;
      if (failed > 5) {
        break;
      }
    }

    machine->step_instruction();
    ++compared;
  }

  std::printf("nestest: %d/%d matched\n", compared - failed, compared);
  return failed == 0 ? 0 : 1;
}
