#include "nesemu/mapper.hpp"

namespace nesemu {
namespace {

class Mapper000 final : public Mapper {
public:
  using Mapper::Mapper;

  u16 mapper_id() const override { return 0; }

  u8 cpu_read(u16 addr) override {
    if (addr >= 0x6000 && addr < 0x8000) {
      return prg_ram_read(addr - 0x6000);
    }
    if (addr >= 0x8000) {
      const std::size_t size = prg_size();
      const std::size_t mask = (size == 0x4000) ? 0x3FFF : 0x7FFF;
      return prg_read(addr & mask);
    }
    return 0;
  }

  void cpu_write(u16 addr, u8 value) override {
    if (addr >= 0x6000 && addr < 0x8000) {
      prg_ram_write(addr - 0x6000, value);
    }
  }

  u8 cpu_peek(u16 addr) const override {
    return const_cast<Mapper000*>(this)->cpu_read(addr);
  }

  u8 ppu_read(u16 addr) override { return chr_read(addr & 0x1FFF); }
  void ppu_write(u16 addr, u8 value) override { chr_write(addr & 0x1FFF, value); }
  u8 ppu_peek(u16 addr) const override { return chr_read(addr & 0x1FFF); }

  void save_state(StateWriter&) const override {}
  void load_state(StateReader&) override {}
};

}  // namespace

std::unique_ptr<Mapper> make_mapper000(Cartridge& c) {
  return std::make_unique<Mapper000>(c);
}

}  // namespace nesemu
