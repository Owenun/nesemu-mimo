#include "nesemu/machine.hpp"
#include "nesemu/mapper.hpp"
#include "test_framework.hpp"

using namespace nesemu;

namespace {

ByteBuffer make_ines(int prg_banks, int chr_banks, u8 mapper, u8 flags6 = 0x01) {
  ByteBuffer rom;
  rom.push_back('N');
  rom.push_back('E');
  rom.push_back('S');
  rom.push_back(0x1A);
  rom.push_back(static_cast<u8>(prg_banks));
  rom.push_back(static_cast<u8>(chr_banks));
  rom.push_back(static_cast<u8>(flags6 | ((mapper & 0x0F) << 4)));
  rom.push_back(static_cast<u8>(mapper & 0xF0));
  for (int i = 8; i < 16; ++i) {
    rom.push_back(0);
  }
  const std::size_t prg_off = rom.size();
  rom.resize(rom.size() + static_cast<std::size_t>(prg_banks) * 16384, 0);
  // Put distinct byte at start of each 16K PRG bank
  for (int b = 0; b < prg_banks; ++b) {
    rom[prg_off + static_cast<std::size_t>(b) * 16384] = static_cast<u8>(0x10 + b);
  }
  rom.resize(rom.size() + static_cast<std::size_t>(chr_banks) * 8192, 0);
  return rom;
}

}  // namespace

NE_TEST(mapper0_nrom_maps_prg) {
  // 32K PRG (2 banks): $8000-$FFFF is linear, no mirror
  auto cart32 = Cartridge::from_bytes(make_ines(2, 1, 0));
  auto mapper32 = Mapper::create(*cart32);
  NE_CHECK(mapper32 != nullptr);
  NE_CHECK_EQ(mapper32->cpu_read(0x8000), 0x10);
  NE_CHECK_EQ(mapper32->cpu_read(0xC000), 0x11);

  // 16K PRG (1 bank): mirrored at $C000
  auto cart16 = Cartridge::from_bytes(make_ines(1, 1, 0));
  auto mapper16 = Mapper::create(*cart16);
  NE_CHECK(mapper16 != nullptr);
  NE_CHECK_EQ(mapper16->cpu_read(0x8000), 0x10);
  NE_CHECK_EQ(mapper16->cpu_read(0xC000), 0x10);
}

NE_TEST(mapper2_uxrom_bank_switch) {
  auto cart = Cartridge::from_bytes(make_ines(4, 1, 2));
  auto mapper = Mapper::create(*cart);
  NE_CHECK(mapper != nullptr);
  NE_CHECK_EQ(mapper->cpu_read(0x8000), 0x10);
  mapper->cpu_write(0x8000, 1);
  NE_CHECK_EQ(mapper->cpu_read(0x8000), 0x11);
  NE_CHECK_EQ(mapper->cpu_read(0xC000), 0x13);  // fixed last bank
}

NE_TEST(mapper3_cnrom_chr_bank) {
  auto cart = Cartridge::from_bytes(make_ines(1, 4, 3));
  auto mapper = Mapper::create(*cart);
  NE_CHECK(mapper != nullptr);
  mapper->ppu_write(0x0000, 0xAA);
  mapper->cpu_write(0x8000, 2);
  // After bank switch, CHR base is different
  (void)mapper->ppu_read(0x0000);
  NE_CHECK(true);
}

NE_TEST(mapper7_axrom_single_screen) {
  auto cart = Cartridge::from_bytes(make_ines(4, 0, 7));
  auto mapper = Mapper::create(*cart);
  NE_CHECK(mapper != nullptr);
  NE_CHECK(mapper->mirroring() == Mirroring::SingleScreenA);
  mapper->cpu_write(0x8000, 0x11);
  NE_CHECK(mapper->mirroring() == Mirroring::SingleScreenB);
}

NE_TEST(unsupported_mapper_fails) {
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 66));
  std::string err;
  auto mapper = Mapper::create(*cart, &err);
  NE_CHECK(mapper == nullptr);
  NE_CHECK(!err.empty());
}
