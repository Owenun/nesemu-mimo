#include "nesemu/apu.hpp"
#include "test_framework.hpp"

using namespace nesemu;

NE_TEST(apu_length_counter) {
  Apu apu;
  apu.reset();
  apu.write_reg(0x4015, 0x01);  // enable pulse1
  apu.write_reg(0x4003, 0x08);  // length = 10 (index 1), start envelope
  // length table[1] = 254 for (0x08>>3)=1
  NE_CHECK(apu.read_status_peek() & 0x01);
}

NE_TEST(apu_status_read_clears_frame_irq) {
  Apu apu;
  apu.reset();
  // 4-step mode will set frame IRQ after ~29830 CPU cycles
  apu.write_reg(0x4017, 0x00);
  for (int i = 0; i < 30000; ++i) {
    apu.clock_cpu();
  }
  const u8 st = apu.read_status();
  NE_CHECK((st & 0x40) != 0);
  const u8 st2 = apu.read_status();
  NE_CHECK((st2 & 0x40) == 0);
}
