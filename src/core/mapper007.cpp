#include "nesemu/mapper.hpp"

namespace nesemu {
namespace {

class Mapper007 final : public Mapper {
public:
  using Mapper::Mapper;

  u16 mapper_id() const override { return 7; }

  Mirroring mirroring() const override {
    return single_ ? Mirroring::SingleScreenB : Mirroring::SingleScreenA;
  }

  u8 cpu_read(u16 addr) override {
    if (addr >= 0x6000 && addr < 0x8000) {
      return prg_ram_read(addr - 0x6000);
    }
    if (addr < 0x8000) {
      return 0;
    }
    const std::size_t size = prg_size();
    const std::size_t bank32 = size / 0x8000;
    const std::size_t bank = bank32 ? (bank_ % bank32) : 0;
    return prg_read(bank * 0x8000 + (addr & 0x7FFF));
  }

  void cpu_write(u16 addr, u8 value) override {
    if (addr >= 0x6000 && addr < 0x8000) {
      prg_ram_write(addr - 0x6000, value);
      return;
    }
    if (addr >= 0x8000) {
      bank_ = static_cast<u8>(value & 0x07);
      single_ = (value & 0x10) != 0;
    }
  }

  u8 cpu_peek(u16 addr) const override {
    return const_cast<Mapper007*>(this)->cpu_read(addr);
  }

  u8 ppu_read(u16 addr) override { return chr_read(addr & 0x1FFF); }
  void ppu_write(u16 addr, u8 value) override { chr_write(addr & 0x1FFF, value); }
  u8 ppu_peek(u16 addr) const override { return chr_read(addr & 0x1FFF); }

  void save_state(StateWriter& w) const override {
    w.write_u8(bank_);
    w.write_bool(single_);
  }

  void load_state(StateReader& r) override {
    bank_ = r.read_u8();
    single_ = r.read_bool();
  }

private:
  u8 bank_ = 0;
  bool single_ = false;
};

}  // namespace

std::unique_ptr<Mapper> make_mapper007(Cartridge& c) {
  return std::make_unique<Mapper007>(c);
}

}  // namespace nesemu
