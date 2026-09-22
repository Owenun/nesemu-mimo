#pragma once

#include "cpu.hpp"
#include "state.hpp"
#include "types.hpp"

#include <array>
#include <functional>

namespace nesemu {

class Mapper;
class Machine;

class SystemBus : public CpuBus {
public:
  void connect(Machine* machine, Mapper* mapper);

  u8 read(u16 addr) override;
  void write(u16 addr, u8 value) override;
  u8 peek(u16 addr) const override;

  void save_state(StateWriter& w) const;
  void load_state(StateReader& r);

  std::array<u8, 0x800>& ram() { return ram_; }
  const std::array<u8, 0x800>& ram() const { return ram_; }

  u8 open_bus() const { return open_bus_; }

  // Controller strobe/shift managed here; Machine feeds button state.
  void set_controller_buttons(u8 player0, u8 player1);
  void write_controller_strobe(u8 value);
  u8 read_controller(u8 player);

private:
  void on_bus_cycle();

  Machine* machine_ = nullptr;
  Mapper* mapper_ = nullptr;
  std::array<u8, 0x800> ram_{};
  u8 open_bus_ = 0;

  u8 controller_strobe_ = 0;
  u8 buttons_[2] = {0, 0};
  u8 shift_[2] = {0, 0};
};

}  // namespace nesemu
