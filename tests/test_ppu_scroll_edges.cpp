#include "nesemu/machine.hpp"
#include "test_framework.hpp"

using namespace nesemu;

namespace {

ByteBuffer make_ines_chr_ram() {
  ByteBuffer rom;
  rom.push_back('N');
  rom.push_back('E');
  rom.push_back('S');
  rom.push_back(0x1A);
  rom.push_back(1);
  rom.push_back(0);  // CHR RAM
  rom.push_back(0);
  rom.push_back(0);
  for (int i = 8; i < 16; ++i) {
    rom.push_back(0);
  }
  rom.resize(rom.size() + 16384, 0);
  return rom;
}

void write_solid_tile(Mapper& m, int tile, u8 lo, u8 hi) {
  const u16 base = static_cast<u16>(tile * 16);
  for (int r = 0; r < 8; ++r) {
    m.ppu_write(static_cast<u16>(base + r), lo);
    m.ppu_write(static_cast<u16>(base + 8 + r), hi);
  }
}

void run_two_frames(Ppu& ppu) {
  // Stop each frame at vblank so framebuffer() is a complete image.
  for (int f = 0; f < 3; ++f) {
    for (int i = 0; i < 341 * 262; ++i) {
      ppu.tick();
      if (ppu.frame_ready()) {
        ppu.clear_frame_ready();
        break;
      }
    }
  }
}

}  // namespace

NE_TEST(ppu_vertical_scroll_top_bottom_rows) {
  auto cart = Cartridge::from_bytes(make_ines_chr_ram());
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();

  write_solid_tile(*mapper, 1, 0xFF, 0x00);  // bg pixel 1
  write_solid_tile(*mapper, 2, 0x00, 0xFF);  // bg pixel 2

  // NT row r: even → tile 1 (palette 0x16), odd → tile 2 (palette 0x21)
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  for (int row = 0; row < 30; ++row) {
    for (int col = 0; col < 32; ++col) {
      ppu.reg_write(0x2007, static_cast<u8>((row & 1) ? 2 : 1));
    }
  }

  ppu.reg_write(0x2006, 0x3F);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x0F);
  ppu.reg_write(0x2007, 0x16);
  ppu.reg_write(0x2007, 0x21);

  ppu.reg_write(0x2000, 0x00);
  ppu.reg_write(0x2001, 0x0A);

  // Y=0
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2005, 0);
  run_two_frames(ppu);
  const auto& fb = ppu.framebuffer();
  NE_CHECK_EQ(fb.pixels[0], 0x16);  // row 0 even
  NE_CHECK_EQ(fb.pixels[static_cast<std::size_t>(239 * 256)], 0x21);  // row 29 odd

  // Y=3: bottom still row 29 (fine 7+3 wraps within row 29? 3+239=242=30*8+2 → NT1 row0)
  // 242 = 30*8+2 → already wraps to NT Y=1. Y=0 is the last scroll where
  // the bottom stays in NT Y=0 (3+239 wait: 0+239=239=29*8+7 in NT0).
  // Y=1: 240=30*8+0 → NT1 row0. So only Y=0 keeps bottom in NT0 row 29.
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2005, 1);
  run_two_frames(ppu);
  NE_CHECK_EQ(fb.pixels[0], 0x16);   // still row 0 (fine 1)
  NE_CHECK_EQ(fb.pixels[static_cast<std::size_t>(239 * 256)], 0x0F);  // wrapped NT1

  // Y=8: top is NT row 1 (odd → 0x21)
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2005, 8);
  run_two_frames(ppu);
  NE_CHECK_EQ(fb.pixels[0], 0x21);

  // Y=232: top is row 29 (odd)
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2005, 232);
  run_two_frames(ppu);
  NE_CHECK_EQ(fb.pixels[0], 0x21);
}

// Scrolling ~200 must not wrap row 29 (building) onto y=0.
NE_TEST(ppu_scroll_no_wrap_bottom_to_top) {
  auto cart = Cartridge::from_bytes(make_ines_chr_ram());
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();
  write_solid_tile(*mapper, 1, 0xFF, 0x00);
  write_solid_tile(*mapper, 2, 0x00, 0xFF);
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  for (int row = 0; row < 30; ++row) {
    for (int col = 0; col < 32; ++col) {
      ppu.reg_write(0x2007, static_cast<u8>((row == 29) ? 2 : 1));
    }
  }
  ppu.reg_write(0x2006, 0x24);
  ppu.reg_write(0x2006, 0x00);
  for (int i = 0; i < 30 * 32; ++i) {
    ppu.reg_write(0x2007, 1);
  }
  ppu.reg_write(0x2006, 0x3F);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x0F);
  ppu.reg_write(0x2007, 0x16);
  ppu.reg_write(0x2007, 0x21);
  ppu.reg_write(0x2000, 0x00);
  ppu.reg_write(0x2001, 0x0A);

  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2005, 200);
  run_two_frames(ppu);
  const auto& fb = ppu.framebuffer();
  // Building (row 29) must not appear at the top after scrolling to Y=200.
  NE_CHECK_EQ(fb.pixels[0], 0x16);
  NE_CHECK_EQ(fb.pixels[static_cast<std::size_t>(8 * 256)], 0x16);
  NE_CHECK_EQ(fb.pixels[static_cast<std::size_t>(16 * 256)], 0x16);
}
