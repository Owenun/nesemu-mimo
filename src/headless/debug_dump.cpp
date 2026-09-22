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

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: nesemu_debug <rom> [frames]\n");
    return 2;
  }
  int frames = 180;
  if (argc >= 3) {
    frames = std::atoi(argv[2]);
  }
  auto machine = nesemu::Machine::load_rom(read_file(argv[1]));
  if (!machine) {
    return 1;
  }
  for (int i = 0; i < frames; ++i) {
    machine->run_until_frame();
  }
  auto& cpu = machine->cpu();
  auto& ppu = machine->ppu();
  std::printf("PC=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X CYC=%llu\n", cpu.pc(), cpu.a(), cpu.x(),
              cpu.y(), cpu.sp(), cpu.p(), static_cast<unsigned long long>(cpu.cycles()));
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
