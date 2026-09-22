#include "nesemu/cartridge.hpp"
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
  rom.resize(rom.size() + static_cast<std::size_t>(prg_banks) * 16384, 0);
  rom.resize(rom.size() + static_cast<std::size_t>(chr_banks) * 8192, 0);
  return rom;
}

}  // namespace

NE_TEST(cartridge_parses_ines) {
  auto cart = Cartridge::from_bytes(make_ines(1, 1, 0));
  NE_CHECK(cart.has_value());
  NE_CHECK_EQ(cart->prg_rom().size(), 16384u);
  NE_CHECK_EQ(cart->chr_rom().size(), 8192u);
  NE_CHECK_EQ(cart->mapper_id(), 0);
}

NE_TEST(cartridge_rejects_bad_signature) {
  auto rom = make_ines(1, 1, 0);
  rom[0] = 'X';
  std::string err;
  auto cart = Cartridge::from_bytes(rom, &err);
  NE_CHECK(!cart.has_value());
  NE_CHECK(!err.empty());
}

NE_TEST(cartridge_chr_ram_when_no_chr) {
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 0));
  NE_CHECK(cart.has_value());
  NE_CHECK(cart->chr_ram().size() == 8192u);
  NE_CHECK(cart->uses_chr_ram());
}

NE_TEST(cartridge_battery_ram) {
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 0, 0x03));
  NE_CHECK(cart.has_value());
  NE_CHECK(cart->has_battery());
  u8 data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  cart->battery_load(data, 8);
  NE_CHECK_EQ(cart->prg_ram()[0], 1);
}
