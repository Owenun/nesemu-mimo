#include "nesemu/machine.hpp"
#include "test_framework.hpp"

using namespace nesemu;

namespace {

ByteBuffer make_ines(int prg_banks = 1, int chr_banks = 1, u8 mapper = 0) {
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
  // Reset vector -> $8000
  const std::size_t prg_base = 16;
  rom[prg_base + 0x3FFC] = 0x00;
  rom[prg_base + 0x3FFD] = 0x80;
  // Infinite NOP loop at $8000: JMP $8000
  rom[prg_base + 0x0000] = 0x4C;
  rom[prg_base + 0x0001] = 0x00;
  rom[prg_base + 0x0002] = 0x80;
  return rom;
}

}  // namespace

NE_TEST(frame_cycle_budget_stable) {
  auto m = Machine::load_rom(make_ines());
  NE_CHECK(m != nullptr);
  m->run_until_frame();  // partial first frame from pre-render
  const u64 c0 = m->cpu_cycles();
  m->run_until_frame();
  const u64 c1 = m->cpu_cycles();
  const u64 dc = c1 - c0;
  // NTSC frame ≈ 29780.5 CPU cycles; allow a small window.
  NE_CHECK(dc > 29000 && dc < 31000);
}

NE_TEST(state_round_trip) {
  auto m = Machine::load_rom(make_ines());
  NE_CHECK(m != nullptr);
  m->run_until_frame();
  const ByteBuffer st = m->save_state();
  NE_CHECK(st.size() > 32);

  const u8 a1 = m->cpu().a();
  const u16 pc1 = m->cpu().pc();
  const u64 fp = m->rom_fingerprint();

  // Mutate then restore
  m->cpu().set_a(0x77);
  std::string err;
  NE_CHECK(m->load_state(st, &err));
  NE_CHECK_EQ(m->cpu().a(), a1);
  NE_CHECK_EQ(m->cpu().pc(), pc1);
  NE_CHECK_EQ(m->rom_fingerprint(), fp);
}

NE_TEST(state_rejects_wrong_fingerprint) {
  auto m1 = Machine::load_rom(make_ines(1, 1, 0));
  auto m2 = Machine::load_rom(make_ines(1, 1, 2));
  NE_CHECK(m1 && m2);
  const ByteBuffer st = m1->save_state();
  std::string err;
  NE_CHECK(!m2->load_state(st, &err));
}
