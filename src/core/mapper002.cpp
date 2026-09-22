#include "nesemu/mapper.hpp"

namespace nesemu {
namespace {

class Mapper002 final : public Mapper {
public:
  using Mapper::Mapper;

  u16 mapper_id() const override { return 2; }

  u8 cpu_read(u16 addr) override {
    if (addr >= 0x6000 && addr < 0x8000) {
      return prg_ram_read(addr - 0x6000);
    }
    if (addr < 0x8000) {
      return 0;
    }
    const std::size_t size = prg_size();
    const std::size_t bank_count = size / 0x4000;
    const std::size_t last = bank_count ? bank_count - 1 : 0;
    if (addr < 0xC000) {
      const std::size_t bank = bank_count ? (bank_ % bank_count) : 0;
      return prg_read(bank * 0x4000 + (addr & 0x3FFF));
    }
    return prg_read(last * 0x4000 + (addr & 0x3FFF));
  }

  void cpu_write(u16 addr, u8 value) override {
    if (addr >= 0x6000 && addr < 0x8000) {
      prg_ram_write(addr - 0x6000, value);
      return;
    }
    if (addr >= 0x8000) {
      bank_ = value;
    }
  }

  u8 cpu_peek(u16 addr) const override {
    return const_cast<Mapper002*>(this)->cpu_read(addr);
  }

  u8 ppu_read(u16 addr) override { return chr_read(addr & 0x1FFF); }
  void ppu_write(u16 addr, u8 value) override { chr_write(addr & 0x1FFF, value); }
  u8 ppu_peek(u16 addr) const override { return chr_read(addr & 0x1FFF); }

  void save_state(StateWriter& w) const override { w.write_u8(bank_); }
  void load_state(StateReader& r) override { bank_ = r.read_u8(); }

private:
  u8 bank_ = 0;
};

}  // namespace

std::unique_ptr<Mapper> make_mapper002(Cartridge& c) {
  return std::make_unique<Mapper002>(c);
}

}  // namespace nesemu
