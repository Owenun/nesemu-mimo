#pragma once

#include "cartridge.hpp"
#include "state.hpp"
#include "types.hpp"

#include <memory>

namespace nesemu {

class Mapper {
public:
  explicit Mapper(Cartridge& cart) : cart_(cart) {}
  virtual ~Mapper() = default;

  virtual u16 mapper_id() const = 0;

  // CPU $4020-$FFFF (and typically $8000+ for ROM).
  virtual u8 cpu_read(u16 addr) = 0;
  virtual void cpu_write(u16 addr, u8 value) = 0;
  virtual u8 cpu_peek(u16 addr) const = 0;

  // PPU $0000-$1FFF pattern tables.
  virtual u8 ppu_read(u16 addr) = 0;
  virtual void ppu_write(u16 addr, u8 value) = 0;
  virtual u8 ppu_peek(u16 addr) const = 0;

  virtual Mirroring mirroring() const;
  virtual bool irq_line() const { return false; }
  virtual void clock_ppu() {}
  virtual void observe_ppu_address(u16 addr) { (void)addr; }
  // MMC3-style scanline counter: one clock per rendered line (stable split).
  virtual void clock_scanline_irq() {}

  virtual void save_state(StateWriter& w) const = 0;
  virtual void load_state(StateReader& r) = 0;

  virtual void dump_debug(char* buf, unsigned cap) const {
    if (buf && cap) {
      buf[0] = 0;
    }
  }

  Cartridge& cart() { return cart_; }
  const Cartridge& cart() const { return cart_; }

  static std::unique_ptr<Mapper> create(Cartridge& cart, std::string* error = nullptr);

protected:
  u8 prg_read(std::size_t index) const;
  u8 chr_read(std::size_t index) const;
  void chr_write(std::size_t index, u8 value);
  std::size_t prg_size() const { return cart_.prg_rom().size(); }
  std::size_t chr_size() const {
    return cart_.chr_rom().empty() ? cart_.chr_ram().size() : cart_.chr_rom().size();
  }
  std::size_t prg_ram_size() const { return cart_.prg_ram().size(); }
  u8 prg_ram_read(std::size_t index) const;
  void prg_ram_write(std::size_t index, u8 value);

  Cartridge& cart_;
};

}  // namespace nesemu
