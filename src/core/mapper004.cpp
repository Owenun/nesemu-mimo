#include "nesemu/mapper.hpp"

namespace nesemu {
namespace {

class Mapper004 final : public Mapper {
public:
  using Mapper::Mapper;

  u16 mapper_id() const override { return 4; }

  Mirroring mirroring() const override {
    return mirror_ ? Mirroring::Horizontal : Mirroring::Vertical;
  }

  bool irq_line() const override { return irq_pending_; }

  void observe_ppu_address(u16 addr) override {
    const bool a12 = (addr & 0x1000) != 0;
    if (!a12) {
      if (a12_low_dots_ < 255) {
        ++a12_low_dots_;
      }
    }
    if (a12 && !a12_high_) {
      if (a12_low_dots_ >= 2) {
        clock_irq();
      }
      a12_low_dots_ = 0;
    }
    if (a12) {
      a12_low_dots_ = 0;
    }
    a12_high_ = a12;
  }

  void clock_ppu() override {
    if (!a12_high_ && a12_low_dots_ < 255) {
      ++a12_low_dots_;
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
    const bool even = (addr & 0x0001) == 0;
    switch (addr & 0xE001) {
      case 0x8000:
        bank_select_ = value;
        break;
      case 0x8001:
        banks_[bank_select_ & 0x07] = value;
        break;
      case 0xA000:
        mirror_ = (value & 0x01) != 0;
        break;
      case 0xA001:
        prg_ram_protect_ = value;
        break;
      case 0xC000:
        irq_latch_ = value;
        break;
      case 0xC001:
        irq_reload_ = true;
        break;
      case 0xE000:
        irq_enabled_ = false;
        irq_pending_ = false;
        break;
      case 0xE001:
        irq_enabled_ = true;
        break;
      default:
        (void)even;
        break;
    }
  }

  u8 cpu_peek(u16 addr) const override {
    return const_cast<Mapper004*>(this)->cpu_read(addr);
  }

  u8 ppu_read(u16 addr) override {
    return chr_read(chr_offset(addr));
  }

  void ppu_write(u16 addr, u8 value) override {
    chr_write(chr_offset(addr), value);
  }

  u8 ppu_peek(u16 addr) const override {
    return chr_read(chr_offset(addr));
  }

  void save_state(StateWriter& w) const override {
    for (int i = 0; i < 8; ++i) {
      w.write_u8(banks_[i]);
    }
    w.write_u8(bank_select_);
    w.write_u8(irq_latch_);
    w.write_u8(irq_counter_);
    w.write_bool(irq_reload_);
    w.write_bool(irq_enabled_);
    w.write_bool(irq_pending_);
    w.write_bool(mirror_);
    w.write_u8(prg_ram_protect_);
    w.write_bool(a12_high_);
    w.write_u8(a12_low_dots_);
    w.write_u8(filter_counter_);
  }

  void load_state(StateReader& r) override {
    for (int i = 0; i < 8; ++i) {
      banks_[i] = r.read_u8();
    }
    bank_select_ = r.read_u8();
    irq_latch_ = r.read_u8();
    irq_counter_ = r.read_u8();
    irq_reload_ = r.read_bool();
    irq_enabled_ = r.read_bool();
    irq_pending_ = r.read_bool();
    mirror_ = r.read_bool();
    prg_ram_protect_ = r.read_u8();
    a12_high_ = r.read_bool();
    a12_low_dots_ = r.read_u8();
    filter_counter_ = r.read_u8();
  }

private:
  void clock_irq() {
    ++total_clocks_;
    if (irq_counter_ == 0 || irq_reload_) {
      irq_counter_ = irq_latch_;
      irq_reload_ = false;
    } else {
      --irq_counter_;
    }
    if (irq_counter_ == 0 && irq_enabled_) {
      irq_pending_ = true;
    }
  }

  u32 total_clocks() const { return total_clocks_; }

  std::size_t prg_offset(u16 addr) const {
    const std::size_t size = prg_size();
    const std::size_t bank8 = size / 0x2000;
    if (bank8 == 0) {
      return 0;
    }
    const bool prg_mode = (bank_select_ & 0x40) != 0;
    const u8* b = banks_;
    std::size_t slot = 0;  // which 8K within $8000-$FFFF
    if (addr < 0xA000) {
      slot = prg_mode ? 6 : b[6];
    } else if (addr < 0xC000) {
      slot = b[7];
    } else if (addr < 0xE000) {
      slot = prg_mode ? b[6] : (bank8 - 2);
    } else {
      slot = bank8 - 1;
    }
    // Use last two banks when fixed indices are hard-coded as 6/7 and bank8 small.
    if (!prg_mode && addr >= 0xC000 && addr < 0xE000) {
      slot = bank8 >= 2 ? bank8 - 2 : 0;
    }
    if (prg_mode && addr < 0xA000) {
      slot = bank8 >= 2 ? bank8 - 2 : 0;
    }
    if (addr >= 0xE000) {
      slot = bank8 ? bank8 - 1 : 0;
    }
    slot %= bank8;
    return slot * 0x2000 + (addr & 0x1FFF);
  }

  std::size_t chr_offset(u16 addr) const {
    const std::size_t size = chr_size();
    if (size == 0) {
      return 0;
    }
    const bool inv = (bank_select_ & 0x80) != 0;
    const u8* b = banks_;
    std::size_t off = 0;
    if (!inv) {
      if (addr < 0x0800) {
        off = static_cast<std::size_t>(b[0] & 0xFE) * 0x0400 + (addr & 0x07FF);
      } else if (addr < 0x1000) {
        off = static_cast<std::size_t>(b[1] & 0xFE) * 0x0400 + (addr & 0x07FF);
      } else if (addr < 0x1400) {
        off = static_cast<std::size_t>(b[2]) * 0x0400 + (addr & 0x03FF);
      } else if (addr < 0x1800) {
        off = static_cast<std::size_t>(b[3]) * 0x0400 + (addr & 0x03FF);
      } else if (addr < 0x1C00) {
        off = static_cast<std::size_t>(b[4]) * 0x0400 + (addr & 0x03FF);
      } else {
        off = static_cast<std::size_t>(b[5]) * 0x0400 + (addr & 0x03FF);
      }
    } else {
      if (addr < 0x0400) {
        off = static_cast<std::size_t>(b[2]) * 0x0400 + (addr & 0x03FF);
      } else if (addr < 0x0800) {
        off = static_cast<std::size_t>(b[3]) * 0x0400 + (addr & 0x03FF);
      } else if (addr < 0x0C00) {
        off = static_cast<std::size_t>(b[4]) * 0x0400 + (addr & 0x03FF);
      } else if (addr < 0x1000) {
        off = static_cast<std::size_t>(b[5]) * 0x0400 + (addr & 0x03FF);
      } else if (addr < 0x1800) {
        off = static_cast<std::size_t>(b[0] & 0xFE) * 0x0400 + (addr & 0x07FF);
      } else {
        off = static_cast<std::size_t>(b[1] & 0xFE) * 0x0400 + (addr & 0x07FF);
      }
    }
    return off % size;
  }

  u8 banks_[8] = {0, 2, 4, 5, 6, 7, 0, 1};
  u8 bank_select_ = 0;
  u8 irq_latch_ = 0;
  u8 irq_counter_ = 0;
  bool irq_reload_ = false;
  bool irq_enabled_ = false;
  bool irq_pending_ = false;
  bool mirror_ = false;  // false=vertical, true=horizontal
  u8 prg_ram_protect_ = 0;
  bool a12_high_ = false;
  u8 a12_low_dots_ = 255;
  u8 filter_counter_ = 0;
  u32 total_clocks_ = 0;
};

}  // namespace

std::unique_ptr<Mapper> make_mapper004(Cartridge& c) {
  return std::make_unique<Mapper004>(c);
}

}  // namespace nesemu
