#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nesemu {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using ByteBuffer = std::vector<u8>;

constexpr int kScreenWidth = 256;
constexpr int kScreenHeight = 240;
constexpr int kCpuHz = 1789773;
constexpr int kApuSampleRate = 44100;

enum class Mirroring : u8 {
  Horizontal,
  Vertical,
  SingleScreenA,
  SingleScreenB,
  FourScreen,
};

enum class TvSystem : u8 {
  Ntsc,
  Pal,
  Multi,
  Dendy,
};

enum class ConsoleType : u8 {
  NesFamicom,
  VsSystem,
  Playchoice10,
  Extended,
};

struct RomHeader {
  u8 prg_rom_units = 0;  // 16 KiB units
  u8 chr_rom_units = 0;  // 8 KiB units
  u8 mapper_low = 0;
  u8 mapper_high = 0;
  u8 prg_ram_units = 0;
  u8 chr_ram_units = 0;
  bool has_trainer = false;
  bool has_battery = false;
  bool nes2 = false;
  u8 submapper = 0;
  u32 prg_rom_size = 0;
  u32 chr_rom_size = 0;
  u32 prg_ram_size = 0;
  u32 prg_nvram_size = 0;
  u32 chr_ram_size = 0;
  u32 chr_nvram_size = 0;
  Mirroring mirroring = Mirroring::Horizontal;
  TvSystem tv_system = TvSystem::Ntsc;
  ConsoleType console_type = ConsoleType::NesFamicom;
  u16 mapper_id = 0;
};

enum class Button : u8 {
  A = 0,
  B,
  Select,
  Start,
  Up,
  Down,
  Left,
  Right,
};

constexpr u8 button_bit(Button b) {
  return static_cast<u8>(1u << static_cast<u8>(b));
}

struct ControllerState {
  u8 buttons = 0;

  void set(Button b, bool pressed) {
    if (pressed) {
      buttons = static_cast<u8>(buttons | button_bit(b));
    } else {
      buttons = static_cast<u8>(buttons & ~button_bit(b));
    }
  }

  bool pressed(Button b) const {
    return (buttons & button_bit(b)) != 0;
  }
};

// 256x240 NES palette indices + emphasis bits in high nibble of second plane.
struct Framebuffer {
  std::array<u8, kScreenWidth * kScreenHeight> pixels{};  // palette index 0-63
  std::array<u8, kScreenWidth * kScreenHeight> emphasis{};  // bits 0-2 RGB emphasis

  void clear() {
    pixels.fill(0);
    emphasis.fill(0);
  }

  void set(int x, int y, u8 color, u8 emph) {
    const int i = y * kScreenWidth + x;
    pixels[static_cast<std::size_t>(i)] = static_cast<u8>(color & 0x3F);
    emphasis[static_cast<std::size_t>(i)] = static_cast<u8>(emph & 0x07);
  }
};

}  // namespace nesemu
