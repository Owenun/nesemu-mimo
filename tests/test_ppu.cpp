#include "nesemu/machine.hpp"
#include "nesemu/ppu.hpp"
#include "test_framework.hpp"

using namespace nesemu;

NE_TEST(ppu_vblank_sets_status) {
  Ppu ppu;
  ppu.reset();
  // From pre-render (261,0) to (241,1) is enough dots for vblank set.
  for (int i = 0; i < 262 * 341; ++i) {
    ppu.tick();
    if (ppu.frame_ready()) {
      break;
    }
  }
  NE_CHECK(ppu.frame_ready());
  const u8 st = ppu.reg_read(0x2002);
  NE_CHECK((st & 0x80) != 0);
  // Reading again clears vblank
  const u8 st2 = ppu.reg_read(0x2002);
  NE_CHECK((st2 & 0x80) == 0);
}

NE_TEST(ppu_2002_clears_vblank_and_toggle) {
  Ppu ppu;
  ppu.reset();
  ppu.reg_write(0x2006, 0x20);
  ppu.reg_write(0x2006, 0x00);
  (void)ppu.reg_read(0x2002);
  // After status read, write toggle cleared: two writes to 0x2006 set full addr
  ppu.reg_write(0x2006, 0x21);
  ppu.reg_write(0x2006, 0x08);
  ppu.reg_write(0x2007, 0xAB);
  ppu.reg_write(0x2006, 0x21);
  ppu.reg_write(0x2006, 0x08);
  (void)ppu.reg_read(0x2007);  // buffer
  const u8 v = ppu.reg_read(0x2007);
  NE_CHECK_EQ(v, 0xAB);
}
