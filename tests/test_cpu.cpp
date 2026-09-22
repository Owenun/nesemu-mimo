#include "nesemu/cpu.hpp"
#include "test_framework.hpp"

#include <array>

using namespace nesemu;

namespace {

class RamBus final : public CpuBus {
public:
  u8 read(u16 addr) override {
    return mem[addr];
  }
  void write(u16 addr, u8 value) override {
    mem[addr] = value;
  }
  u8 peek(u16 addr) const override {
    return mem[addr];
  }
  std::array<u8, 0x10000> mem{};
};

}  // namespace

NE_TEST(cpu_lda_imm) {
  RamBus bus;
  Cpu cpu;
  cpu.connect(&bus);
  bus.mem[0x0000] = 0xA9;
  bus.mem[0x0001] = 0x42;
  bus.mem[0xFFFC] = 0x00;
  bus.mem[0xFFFD] = 0x00;
  cpu.reset();
  cpu.set_pc(0x0000);
  cpu.step();
  NE_CHECK_EQ(cpu.a(), 0x42);
  NE_CHECK_EQ(cpu.pc(), 0x0002);
  NE_CHECK(!cpu.flag(Cpu::Z));
  NE_CHECK(!cpu.flag(Cpu::N));
}

NE_TEST(cpu_jmp_abs) {
  RamBus bus;
  Cpu cpu;
  cpu.connect(&bus);
  bus.mem[0x0000] = 0x4C;
  bus.mem[0x0001] = 0x34;
  bus.mem[0x0002] = 0x12;
  cpu.reset();
  cpu.set_pc(0x0000);
  cpu.step();
  NE_CHECK_EQ(cpu.pc(), 0x1234);
}

NE_TEST(cpu_jmp_indirect_bug) {
  RamBus bus;
  Cpu cpu;
  cpu.connect(&bus);
  bus.mem[0x0000] = 0x6C;
  bus.mem[0x0001] = 0xFF;
  bus.mem[0x0002] = 0x02;  // ($02FF) -> bug reads $02FF and $0200
  bus.mem[0x02FF] = 0x34;
  bus.mem[0x0200] = 0x12;
  bus.mem[0x0300] = 0x99;  // must NOT be used
  cpu.reset();
  cpu.set_pc(0x0000);
  cpu.step();
  NE_CHECK_EQ(cpu.pc(), 0x1234);
}

NE_TEST(cpu_jsr_rts) {
  RamBus bus;
  Cpu cpu;
  cpu.connect(&bus);
  bus.mem[0x0000] = 0x20;
  bus.mem[0x0001] = 0x00;
  bus.mem[0x0002] = 0x10;  // JSR $1000
  bus.mem[0x1000] = 0x60;  // RTS
  cpu.reset();
  cpu.set_pc(0x0000);
  cpu.set_sp(0xFD);
  cpu.step();
  NE_CHECK_EQ(cpu.pc(), 0x1000);
  cpu.step();
  NE_CHECK_EQ(cpu.pc(), 0x0003);
}

NE_TEST(cpu_adc_carry_overflow) {
  RamBus bus;
  Cpu cpu;
  cpu.connect(&bus);
  cpu.reset();
  cpu.set_a(0x50);
  cpu.set_flag(Cpu::C, false);
  bus.mem[0x0000] = 0x69;
  bus.mem[0x0001] = 0x50;
  cpu.set_pc(0x0000);
  cpu.step();
  NE_CHECK_EQ(cpu.a(), 0xA0);
  NE_CHECK(cpu.flag(Cpu::N));
  NE_CHECK(cpu.flag(Cpu::V));
  NE_CHECK(!cpu.flag(Cpu::C));
}

NE_TEST(cpu_abs_x_sta_lda) {
  RamBus bus;
  Cpu cpu;
  cpu.connect(&bus);
  cpu.reset();
  // LDX #$04; LDA #$AA; STA $1234,X; LDA $1234,X
  bus.mem[0x0000] = 0xA2;
  bus.mem[0x0001] = 0x04;
  bus.mem[0x0002] = 0xA9;
  bus.mem[0x0003] = 0xAA;
  bus.mem[0x0004] = 0x9D;
  bus.mem[0x0005] = 0x34;
  bus.mem[0x0006] = 0x12;
  bus.mem[0x0007] = 0xBD;
  bus.mem[0x0008] = 0x34;
  bus.mem[0x0009] = 0x12;
  cpu.set_pc(0x0000);
  cpu.step();  // LDX
  cpu.step();  // LDA
  cpu.step();  // STA
  NE_CHECK_EQ(bus.mem[0x1238], 0xAA);
  cpu.set_a(0);
  cpu.step();  // LDA
  NE_CHECK_EQ(cpu.a(), 0xAA);
}

NE_TEST(cpu_lda_abs_x_page_cross) {
  RamBus bus;
  Cpu cpu;
  cpu.connect(&bus);
  cpu.reset();
  bus.mem[0x0000] = 0xA2;
  bus.mem[0x0001] = 0x10;
  bus.mem[0x0002] = 0xBD;
  bus.mem[0x0003] = 0xFF;
  bus.mem[0x0004] = 0x10;  // $10FF+X = $110F (page cross)
  bus.mem[0x110F] = 0x5A;
  cpu.set_pc(0x0000);
  cpu.step();
  cpu.step();
  NE_CHECK_EQ(cpu.a(), 0x5A);
}

NE_TEST(cpu_indirect_indexed) {
  RamBus bus;
  Cpu cpu;
  cpu.connect(&bus);
  cpu.reset();
  bus.mem[0x0020] = 0x00;
  bus.mem[0x0021] = 0x30;  // pointer $3000
  bus.mem[0x0000] = 0xA0;
  bus.mem[0x0001] = 0x05;  // LDY #$05
  bus.mem[0x0002] = 0xB1;
  bus.mem[0x0003] = 0x20;  // LDA ($20),Y
  bus.mem[0x3005] = 0x77;
  cpu.set_pc(0x0000);
  cpu.step();
  cpu.step();
  NE_CHECK_EQ(cpu.a(), 0x77);
}

