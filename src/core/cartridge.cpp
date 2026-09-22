#include "nesemu/cartridge.hpp"

#include <cstdio>
#include <cstring>

namespace nesemu {
namespace {

bool checked_mul(std::size_t a, std::size_t b, std::size_t* out) {
  if (a != 0 && b > (SIZE_MAX / a)) {
    return false;
  }
  *out = a * b;
  return true;
}

bool checked_add(std::size_t a, std::size_t b, std::size_t* out) {
  if (b > SIZE_MAX - a) {
    return false;
  }
  *out = a + b;
  return true;
}

u64 fnv1a(const u8* data, std::size_t size) {
  u64 h = 14695981039346656037ull;
  for (std::size_t i = 0; i < size; ++i) {
    h ^= data[i];
    h *= 1099511628211ull;
  }
  return h;
}

}  // namespace

std::optional<Cartridge> Cartridge::from_bytes(const ByteBuffer& data, std::string* error) {
  auto fail = [&](const char* msg) -> std::optional<Cartridge> {
    if (error) {
      *error = msg;
    }
    return std::nullopt;
  };

  if (data.size() < 16) {
    return fail("ROM too small for iNES header");
  }
  if (!(data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A)) {
    return fail("missing NES<EOF> signature");
  }

  Cartridge cart;
  RomHeader& h = cart.header_;

  const u8 b4 = data[4];
  const u8 b5 = data[5];
  const u8 b6 = data[6];
  const u8 b7 = data[7];
  const u8 b8 = data[8];
  const u8 b9 = data[9];
  const u8 b10 = data[10];
  const u8 b11 = data[11];
  const u8 b12 = data[12];
  const u8 b13 = data[13];

  const u8 ines2_sig = static_cast<u8>(b7 & 0x0C);
  h.nes2 = (ines2_sig == 0x08);
  h.prg_rom_units = b4;
  h.chr_rom_units = b5;
  h.has_trainer = (b6 & 0x04) != 0;
  h.has_battery = (b6 & 0x02) != 0;

  if (h.nes2) {
    h.mapper_id = static_cast<u16>((b6 >> 4) | ((b7 & 0xF0)) | ((b8 & 0x0F) << 8));
    h.submapper = static_cast<u8>(b8 >> 4);
    h.prg_rom_size = static_cast<u32>(((b9 & 0x0F) << 8) | b4) * 0x4000;
    h.chr_rom_size = static_cast<u32>(((b9 & 0xF0) << 4) | b5) * 0x2000;
    // Shift counts in b10/b11 for RAM sizes: size = 64 << n when n>0
    auto ram_size = [](u8 nibble, bool nvram) -> u32 {
      (void)nvram;
      if (nibble == 0) {
        return 0;
      }
      return 64u << nibble;
    };
    h.prg_ram_size = ram_size(static_cast<u8>(b10 & 0x0F), false);
    h.prg_nvram_size = ram_size(static_cast<u8>(b10 >> 4), true);
    h.chr_ram_size = ram_size(static_cast<u8>(b11 & 0x0F), false);
    h.chr_nvram_size = ram_size(static_cast<u8>(b11 >> 4), true);
    const u8 timing = static_cast<u8>(b12 & 0x03);
    h.tv_system = timing == 0 ? TvSystem::Ntsc
                 : timing == 1 ? TvSystem::Pal
                 : timing == 2 ? TvSystem::Multi
                               : TvSystem::Dendy;
    const u8 console = static_cast<u8>(b13 & 0x03);
    h.console_type = console == 0   ? ConsoleType::NesFamicom
                     : console == 1 ? ConsoleType::VsSystem
                     : console == 2 ? ConsoleType::Playchoice10
                                    : ConsoleType::Extended;
    h.prg_ram_units = 0;
    h.chr_ram_units = 0;
  } else {
    h.mapper_id = static_cast<u16>((b6 >> 4) | (b7 & 0xF0));
    h.submapper = 0;
    h.prg_rom_size = static_cast<u32>(b4) * 0x4000;
    h.chr_rom_size = static_cast<u32>(b5) * 0x2000;
    h.prg_ram_units = b8;
    h.chr_ram_units = b9;
    const u32 prg_ram = (b8 == 0) ? 0x2000u : static_cast<u32>(b8) * 0x2000u;
    h.prg_ram_size = prg_ram;
    h.prg_nvram_size = h.has_battery ? prg_ram : 0;
    h.chr_ram_size = (b5 == 0) ? 0x2000u : 0;
    h.chr_nvram_size = 0;
    h.tv_system = (b9 & 0x01) ? TvSystem::Pal : TvSystem::Ntsc;
    h.console_type = ConsoleType::NesFamicom;
    // Some dumps mark four-screen in bit 3 of flags6 (already in b6).
  }

  // flags6 bit3 = four-screen (iNES) / mirroring bit for some boards
  const bool four_screen = (b6 & 0x08) != 0;
  (void)b13;
  (void)b12;

  if (four_screen) {
    h.mirroring = Mirroring::FourScreen;
  } else if (b6 & 0x01) {
    h.mirroring = Mirroring::Vertical;
  } else {
    h.mirroring = Mirroring::Horizontal;
  }

  if (h.console_type == ConsoleType::VsSystem) {
    return fail("VS System ROMs are not supported");
  }
  if (h.console_type == ConsoleType::Playchoice10) {
    return fail("PlayChoice-10 ROMs are not supported");
  }

  std::size_t offset = 16;
  if (h.has_trainer) {
    if (!checked_add(offset, 512, &offset)) {
      return fail("header overflow");
    }
  }
  std::size_t prg_size = 0;
  std::size_t chr_size = 0;
  if (!checked_mul(h.prg_rom_size, 1, &prg_size)) {
    return fail("PRG size overflow");
  }
  if (!checked_mul(h.chr_rom_size, 1, &chr_size)) {
    return fail("CHR size overflow");
  }
  if (prg_size == 0) {
    return fail("PRG ROM size is zero");
  }

  std::size_t need = 0;
  if (!checked_add(offset, prg_size, &need) || !checked_add(need, chr_size, &need)) {
    return fail("ROM size overflow");
  }
  if (data.size() < need) {
    return fail("ROM file truncated");
  }

  if (h.has_trainer) {
    std::memcpy(cart.trainer_.data(), data.data() + 16, 512);
    offset += 512;
  }

  cart.prg_rom_.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                       data.begin() + static_cast<std::ptrdiff_t>(offset + prg_size));
  offset += prg_size;
  if (chr_size > 0) {
    cart.chr_rom_.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                         data.begin() + static_cast<std::ptrdiff_t>(offset + chr_size));
    offset += chr_size;
  }

  // PRG RAM
  std::size_t prg_ram_bytes = h.prg_ram_size;
  if (prg_ram_bytes == 0 && !h.nes2) {
    prg_ram_bytes = 0x2000;
  }
  cart.prg_ram_.assign(prg_ram_bytes, 0);

  // CHR RAM if no CHR ROM
  if (cart.chr_rom_.empty()) {
    std::size_t chr_ram = h.chr_ram_size;
    if (chr_ram == 0) {
      chr_ram = 0x2000;
    }
    cart.chr_ram_.assign(chr_ram, 0);
  }

  cart.fingerprint_ = fnv1a(data.data(), data.size());
  // Mix header fields so trivial padding differences still key states.
  cart.fingerprint_ ^= static_cast<u64>(h.mapper_id) << 32;
  cart.fingerprint_ ^= h.prg_rom_size;
  cart.fingerprint_ ^= (static_cast<u64>(h.chr_rom_size) << 16);

  return cart;
}

void Cartridge::battery_load(const u8* data, std::size_t size) {
  if (!has_battery() || prg_ram_.empty() || data == nullptr) {
    return;
  }
  const std::size_t n = size < prg_ram_.size() ? size : prg_ram_.size();
  std::memcpy(prg_ram_.data(), data, n);
}

}  // namespace nesemu
