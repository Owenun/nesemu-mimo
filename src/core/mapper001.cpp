#include "nesemu/mapper.hpp"

namespace nesemu {
namespace {

class Mapper001 final : public Mapper {
public:
  using Mapper::Mapper;

  Mapper001(Cartridge& c) : Mapper(c) {
    shift_ = 0x10;
    control_ = 0x0C;
    chr_bank0_ = 0;
    chr_bank1_ = 0;
    prg_bank_ = 0;
  }

  u16 mapper_id() const override { return 1; }

  Mirroring mirroring() const override {
    switch (control_ & 0x03) {
      case 0:
        return Mirroring::SingleScreenA;
      case 1:
        return Mirroring::SingleScreenB;
      case 2:
        return Mirroring::Vertical;
      default:
        return Mirroring::Horizontal;
    }
  }

  u8 cpu_read(u16 addr) override {
    if (addr >= 0x6000 && addr < 0x8000) {
      return prg_ram_read(addr - 0x6000);
    }
    if (addr < 0x8000) {
      return 0;
    }
    return prg_read(prg_offset(addr));
  }

  void cpu_write(u16 addr, u8 value) override {
    if (addr >= 0x6000 && addr < 0x8000) {
      prg_ram_write(addr - 0x6000, value);
      return;
    }
    if (addr < 0x8000) {
      return;
    }
    if (value & 0x80) {
      shift_ = 0x10;
      control_ = static_cast<u8>(control_ | 0x0C);
      return;
    }
    const bool complete = (shift_ & 0x01) != 0;
    shift_ = static_cast<u8>(shift_ >> 1);
    shift_ = static_cast<u8>(shift_ | ((value & 0x01) << 4));
    if (!complete) {
      return;
    }
    const u8 data = shift_;
    shift_ = 0x10;
    switch ((addr >> 13) & 0x03) {
      case 0:
        control_ = data;
        break;
      case 1:
        chr_bank0_ = data;
        break;
      case 2:
        chr_bank1_ = data;
        break;
      case 3:
        prg_bank_ = static_cast<u8>(data & 0x0F);
        break;
    }
  }

  u8 cpu_peek(u16 addr) const override {
    return const_cast<Mapper001*>(this)->cpu_read(addr);
  }

  u8 ppu_read(u16 addr) override { return chr_read(chr_offset(addr)); }
  void ppu_write(u16 addr, u8 value) override { chr_write(chr_offset(addr), value); }
  u8 ppu_peek(u16 addr) const override { return chr_read(chr_offset(addr)); }

  void save_state(StateWriter& w) const override {
    w.write_u8(shift_);
    w.write_u8(control_);
    w.write_u8(chr_bank0_);
    w.write_u8(chr_bank1_);
    w.write_u8(prg_bank_);
  }

  void load_state(StateReader& r) override {
    shift_ = r.read_u8();
    control_ = r.read_u8();
    chr_bank0_ = r.read_u8();
    chr_bank1_ = r.read_u8();
    prg_bank_ = r.read_u8();
  }

private:
  std::size_t chr_offset(u16 addr) const {
    const std::size_t size = chr_size();
    if (size == 0) {
      return 0;
    }
    const u8 mode = static_cast<u8>((control_ >> 4) & 0x01);
    if (mode == 0) {
      const std::size_t bank = (static_cast<std::size_t>(chr_bank0_ & 0x1E) * 0x1000) % size;
      return (bank + (addr & 0x1FFF)) % size;
    }
    if (addr < 0x1000) {
      const std::size_t bank = (static_cast<std::size_t>(chr_bank0_) * 0x1000) % size;
      return (bank + (addr & 0x0FFF)) % size;
    }
    const std::size_t bank = (static_cast<std::size_t>(chr_bank1_) * 0x1000) % size;
    return (bank + (addr & 0x0FFF)) % size;
  }

  std::size_t prg_offset(u16 addr) const {
    const std::size_t size = prg_size();
    const std::size_t bank16 = static_cast<std::size_t>(prg_bank_) * 0x4000;
    const u8 mode = static_cast<u8>((control_ >> 2) & 0x03);
    const std::size_t last = (size / 0x4000) ? (size / 0x4000 - 1) * 0x4000 : 0;
    if (mode == 0 || mode == 1) {
      // 32 KiB mode
      const std::size_t bank = (bank16 & ~std::size_t{0x4000}) % (size ? size : 1);
      return (bank + (addr & 0x7FFF)) % size;
    }
    if (mode == 2) {
      // fix first bank at $8000
      if (addr < 0xC000) {
        return (addr & 0x3FFF) % size;
      }
      return (bank16 + (addr & 0x3FFF)) % size;
    }
    // mode 3: fix last bank at $C000
    if (addr < 0xC000) {
      return (bank16 + (addr & 0x3FFF)) % size;
    }
    return (last + (addr & 0x3FFF)) % size;
  }

  u8 shift_ = 0x10;
  u8 control_ = 0x0C;
  u8 chr_bank0_ = 0;
  u8 chr_bank1_ = 0;
  u8 prg_bank_ = 0;
};

}  // namespace

std::unique_ptr<Mapper> make_mapper001(Cartridge& c) {
  return std::make_unique<Mapper001>(c);
}

}  // namespace nesemu
