#include "nesemu/machine.hpp"

#include <cstdio>
#include <fstream>
#include <string>

using nesemu::u32;

namespace {

nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

u32 fnv1a_frame(const nesemu::Framebuffer& fb) {
  u32 h = 2166136261u;
  for (int i = 0; i < nesemu::kScreenWidth * nesemu::kScreenHeight; ++i) {
    h ^= fb.pixels[static_cast<std::size_t>(i)];
    h *= 16777619u;
  }
  return h;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::printf("usage: nesemu_frame_test <rom> <frames> [expected_hash_hex]\n");
    return 2;
  }
  const int frames = std::atoi(argv[2]);
  auto rom = read_file(argv[1]);
  auto machine = nesemu::Machine::load_rom(rom);
  if (!machine) {
    std::printf("load failed\n");
    return 1;
  }
  for (int i = 0; i < frames; ++i) {
    machine->run_until_frame();
    if (machine->cpu().jammed()) {
      std::printf("jammed at frame %d\n", i);
      return 1;
    }
  }
  const u32 hash = fnv1a_frame(machine->framebuffer());
  std::printf("frame_hash: 0x%08X after %d frames\n", hash, frames);
  if (argc >= 4) {
    const u32 want = static_cast<u32>(std::strtoul(argv[3], nullptr, 16));
    if (want != hash) {
      std::printf("hash mismatch want 0x%08X\n", want);
      return 1;
    }
    std::printf("hash ok\n");
  }
  return 0;
}
