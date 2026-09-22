#include "nesemu/machine.hpp"
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

NE_TEST(ppu_renders_background_tile) {
  // CHR RAM (0 CHR ROM banks) so pattern writes stick
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 0));
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();

  // Solid tile at CHR index 1
  for (int i = 0; i < 8; ++i) {
    mapper->ppu_write(static_cast<u16>(16 + i), 0xFF);
    mapper->ppu_write(static_cast<u16>(16 + 8 + i), 0x00);
  }
  // Palette: bg color 1 = red-ish
  ppu.reg_write(0x2006, 0x3F);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x0F);
  ppu.reg_write(0x2007, 0x16);
  // Nametable: tile 1 at (0,0)
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x01);
  // Rewind VRAM address to NT start and set scroll 0
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2000, 0x00);
  ppu.reg_write(0x2001, 0x0A);  // show bg + left column

  // Run two full frames so prefetch pipeline is primed
  for (int i = 0; i < 2 * 300 * 341; ++i) {
    ppu.tick();
    if (ppu.frame_ready()) {
      ppu.clear_frame_ready();
    }
  }
  const auto& fb = ppu.framebuffer();
  // Pixel (0,0) should use palette index 0x16 (bg pixel 1)
  NE_CHECK_EQ(fb.pixels[0], 0x16);
  // Pixel (8,0) is tile 0 (blank) -> backdrop 0x0F
  NE_CHECK_EQ(fb.pixels[8], 0x0F);
}

NE_TEST(ppu_fine_x_scroll_shifts_bits) {
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 0));
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();

  // Tile 1: left 4 px color1 (0b11110000 lo), right 4 transparent
  for (int i = 0; i < 8; ++i) {
    mapper->ppu_write(static_cast<u16>(16 + i), 0xF0);
    mapper->ppu_write(static_cast<u16>(16 + 8 + i), 0x00);
  }
  // Tile 2 (index 2): solid
  for (int i = 0; i < 8; ++i) {
    mapper->ppu_write(static_cast<u16>(32 + i), 0xFF);
    mapper->ppu_write(static_cast<u16>(32 + 8 + i), 0x00);
  }
  ppu.reg_write(0x2006, 0x3F);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x0F);
  ppu.reg_write(0x2007, 0x16);  // color 1
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x01);  // tile 1
  ppu.reg_write(0x2007, 0x02);  // tile 2
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  // fine X = 4 → first 4 screen pixels are tile1's right half (transparent),
  // then tile2 solid.
  ppu.reg_write(0x2005, 4);  // fine X = 4
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2000, 0x00);
  ppu.reg_write(0x2001, 0x0A);

  for (int i = 0; i < 2 * 300 * 341; ++i) {
    ppu.tick();
    if (ppu.frame_ready()) {
      ppu.clear_frame_ready();
    }
  }
  const auto& fb = ppu.framebuffer();
  // x=0..3: tile1 bits 4..7 = 0xF0 → pixels 0,0,0,0 wait: 0xF0 = 11110000
  // bit7..0 = 1,1,1,1,0,0,0,0 → fine 4 starts at bit 3..0 = 0,0,0,0
  NE_CHECK_EQ(fb.pixels[0], 0x0F);  // transparent → backdrop
  // x=4: first pixel of tile 2 = solid color 1
  NE_CHECK_EQ(fb.pixels[4], 0x16);
}

NE_TEST(ppu_three_tiles_no_skip) {
  // NT: tile1, tile2, tile3 — pixels 0-7 / 8-15 / 16-23 must each match.
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 0));
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();

  // tile1 = color1, tile2 = color2, tile3 = color3
  auto fill_tile = [&](int tile, u8 lo) {
    for (int i = 0; i < 8; ++i) {
      mapper->ppu_write(static_cast<u16>(tile * 16 + i), lo);
      mapper->ppu_write(static_cast<u16>(tile * 16 + 8 + i), 0x00);
    }
  };
  fill_tile(1, 0xFF);
  fill_tile(2, 0xFF);
  fill_tile(3, 0xFF);
  // hi plane different so palette index differs: use hi=0xFF for tile2
  for (int i = 0; i < 8; ++i) {
    mapper->ppu_write(static_cast<u16>(2 * 16 + 8 + i), 0xFF);  // pixel value 3
    mapper->ppu_write(static_cast<u16>(3 * 16 + i), 0x00);
    mapper->ppu_write(static_cast<u16>(3 * 16 + 8 + i), 0xFF);  // pixel value 2
  }

  ppu.reg_write(0x2006, 0x3F);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x0F);
  ppu.reg_write(0x2007, 0x11);  // pal 1
  ppu.reg_write(0x2007, 0x22);  // pal 2
  ppu.reg_write(0x2007, 0x33);  // pal 3

  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x01);
  ppu.reg_write(0x2007, 0x02);
  ppu.reg_write(0x2007, 0x03);
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2000, 0x00);
  ppu.reg_write(0x2001, 0x0A);

  for (int i = 0; i < 2 * 300 * 341; ++i) {
    ppu.tick();
    if (ppu.frame_ready()) {
      ppu.clear_frame_ready();
    }
  }
  const auto& fb = ppu.framebuffer();
  NE_CHECK_EQ(fb.pixels[0], 0x11);   // tile 1 → pal 1
  NE_CHECK_EQ(fb.pixels[8], 0x33);   // tile 2 → pixel 3 → pal 3
  NE_CHECK_EQ(fb.pixels[16], 0x22);  // tile 3 → pixel 2 → pal 2
}

NE_TEST(ppu_sprite_zero_hit) {
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 0));
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();

  // Tile 1 solid for both BG and sprite
  for (int i = 0; i < 8; ++i) {
    mapper->ppu_write(static_cast<u16>(16 + i), 0xFF);
    mapper->ppu_write(static_cast<u16>(16 + 8 + i), 0x00);
  }
  ppu.reg_write(0x2006, 0x3F);
  ppu.reg_write(0x2006, 0x00);
  for (int i = 0; i < 8; ++i) {
    ppu.reg_write(0x2007, 0x0F);
  }
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  ppu.reg_write(0x2007, 0x01);  // BG tile 1 at 0,0
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);

  // Sprite 0 at Y=0 (visible on scanline 1 due to OAM Y quirk), tile 1
  u8 oam[256];
  for (int i = 0; i < 256; ++i) {
    oam[i] = 0xFF;
  }
  oam[0] = 0;   // Y (appears on scanline 1)
  oam[1] = 1;   // tile
  oam[2] = 0;   // attr
  oam[3] = 0;   // X
  for (int i = 0; i < 256; ++i) {
    ppu.oam_dma_write(oam[i]);
  }
  // Reset OAM addr
  ppu.reg_write(0x2003, 0);

  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2005, 0);
  ppu.reg_write(0x2000, 0x00);
  ppu.reg_write(0x2001, 0x1E);  // show bg+sp, left column

  u8 hit = 0;
  for (int i = 0; i < 2 * 300 * 341; ++i) {
    ppu.tick();
    if (ppu.frame_ready()) {
      // Sample before pre-render clears sprite-zero.
      const u8 st = ppu.reg_read(0x2002);
      if (st & 0x40) {
        hit = 1;
      }
      ppu.clear_frame_ready();
    }
  }
  NE_CHECK(hit != 0);
}
