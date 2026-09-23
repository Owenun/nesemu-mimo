#include "nesemu/mapper.hpp"
#include "nesemu/ppu.hpp"
#include "test_framework.hpp"

using namespace nesemu;

namespace {

ByteBuffer make_ines(int prg_banks, int chr_banks, u8 mapper) {
  ByteBuffer rom;
  rom.push_back('N');
  rom.push_back('E');
  rom.push_back('S');
  rom.push_back(0x1A);
  rom.push_back(static_cast<u8>(prg_banks));
  rom.push_back(static_cast<u8>(chr_banks));
  rom.push_back(static_cast<u8>((mapper & 0x0F) << 4));
  rom.push_back(static_cast<u8>(mapper & 0xF0));
  for (int i = 8; i < 16; ++i) {
    rom.push_back(0);
  }
  rom.resize(rom.size() + static_cast<std::size_t>(prg_banks) * 16384, 0);
  rom.resize(rom.size() + static_cast<std::size_t>(chr_banks) * 8192, 0);
  return rom;
}

void ppu_set_addr(Ppu& ppu, u16 addr) {
  ppu.reg_write(0x2006, static_cast<u8>(addr >> 8));
  ppu.reg_write(0x2006, static_cast<u8>(addr & 0xFF));
}

}  // namespace

NE_TEST(mmc3_irq_clocks_on_a12) {
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 4));
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();

  // Latch=1, reload on next clock, enable IRQ
  mapper->cpu_write(0xC000, 1);  // latch
  mapper->cpu_write(0xC001, 0);  // reload flag
  mapper->cpu_write(0xE001, 0);  // enable

  NE_CHECK(!mapper->irq_line());

  // Scanline clock #1: reload counter=latch=1
  mapper->clock_scanline_irq();
  NE_CHECK(!mapper->irq_line());

  // Scanline clock #2: counter 1→0 → IRQ
  mapper->clock_scanline_irq();
  NE_CHECK(mapper->irq_line());

  mapper->cpu_write(0xE000, 0);  // ack
  NE_CHECK(!mapper->irq_line());
}

NE_TEST(mmc3_irq_latch_zero_fires_immediately) {
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 4));
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();

  mapper->cpu_write(0xC000, 0);  // latch 0
  mapper->cpu_write(0xC001, 0);
  mapper->cpu_write(0xE001, 0);

  mapper->clock_scanline_irq();  // reload 0 → IRQ
  NE_CHECK(mapper->irq_line());
}

NE_TEST(mmc3_irq_ignores_short_a12_glitch) {
  auto cart = Cartridge::from_bytes(make_ines(1, 0, 4));
  auto mapper = Mapper::create(*cart);
  Ppu ppu;
  ppu.connect(mapper.get());
  ppu.reset();

  mapper->cpu_write(0xC000, 0);
  mapper->cpu_write(0xC001, 0);
  mapper->cpu_write(0xE001, 0);

  mapper->clock_scanline_irq();
  NE_CHECK(mapper->irq_line());
  mapper->cpu_write(0xE000, 0);

  // Pattern A12 edges alone must not clock the scanline counter.
  mapper->cpu_write(0xC000, 5);
  mapper->cpu_write(0xC001, 0);
  mapper->cpu_write(0xE001, 0);
  ppu_set_addr(ppu, 0x0000);
  ppu_set_addr(ppu, 0x1000);
  ppu_set_addr(ppu, 0x0000);
  ppu_set_addr(ppu, 0x1000);
  NE_CHECK(!mapper->irq_line());
}
