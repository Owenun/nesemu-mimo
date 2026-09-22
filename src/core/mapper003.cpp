#include "nesemu/mapper.hpp"

namespace nesemu {
namespace {

class Mapper003 final : public Mapper {
public:
  using Mapper::Mapper;

  u16 mapper_id() const override { return 3; }

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
      return;
    }
    if (addr >= 0x8000) {
      chr_bank_ = value;
    }
  }

  u8 cpu_peek(u16 addr) const override {
    return const_cast<Mapper003*>(this)->cpu_read(addr);
  }

  u8 ppu_read(u16 addr) override {
    const std::size_t size = chr_size();
    const std::size_t base = size ? (static_cast<std::size_t>(chr_bank_) * 0x2000) % size : 0;
    return chr_read((base + (addr & 0x1FFF)) % (size ? size : 1));
  }

  void ppu_write(u16 addr, u8 value) override {
    const std::size_t size = chr_size();
    const std::size_t base = size ? (static_cast<std::size_t>(chr_bank_) * 0x2000) % size : 0;
    chr_write((base + (addr & 0x1FFF)) % (size ? size : 1), value);
  }

  u8 ppu_peek(u16 addr) const override {
    return const_cast<Mapper003*>(this)->ppu_read(addr);
  }

  void save_state(StateWriter& w) const override { w.write_u8(chr_bank_); }
  void load_state(StateReader& r) override { chr_bank_ = r.read_u8(); }

private:
  u8 chr_bank_ = 0;
};

}  // namespace

std::unique_ptr<Mapper> make_mapper003(Cartridge& c) {
  return std::make_unique<Mapper003>(c);
}

}  // namespace nesemu
