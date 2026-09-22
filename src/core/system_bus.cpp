#include "nesemu/system_bus.hpp"

#include "nesemu/machine.hpp"
#include "nesemu/mapper.hpp"

namespace nesemu {

void SystemBus::connect(Machine* machine, Mapper* mapper) {
  machine_ = machine;
  mapper_ = mapper;
}

void SystemBus::on_bus_cycle() {
  if (machine_) {
    machine_->on_cpu_bus_cycle();
  }
}

u8 SystemBus::read(u16 addr) {
  on_bus_cycle();
  u8 value = 0;
  if (addr < 0x2000) {
    value = ram_[addr & 0x07FF];
  } else if (addr < 0x4000) {
    value = machine_ ? machine_->ppu().reg_read(addr) : 0;
  } else if (addr == 0x4015) {
    value = machine_ ? machine_->apu().read_status() : 0;
  } else if (addr == 0x4016) {
    value = read_controller(0);
  } else if (addr == 0x4017) {
    value = read_controller(1);
  } else if (addr < 0x4020) {
    // APU open bus / write-only regs
    value = open_bus_;
  } else if (mapper_) {
    value = mapper_->cpu_read(addr);
  }
  // Open bus: low 5 bits of PPU reg reads mix with open bus on real HW;
  // keep last driven bus value.
  open_bus_ = value;
  return value;
}

void SystemBus::write(u16 addr, u8 value) {
  on_bus_cycle();
  open_bus_ = value;
  if (addr < 0x2000) {
    ram_[addr & 0x07FF] = value;
    return;
  }
  if (addr < 0x4000) {
    if (machine_) {
      machine_->ppu().reg_write(addr, value);
    }
    return;
  }
  if (addr == 0x4014) {
    if (machine_) {
      machine_->request_oam_dma(value);
    }
    return;
  }
  if (addr == 0x4016) {
    write_controller_strobe(value);
    return;
  }
  if (addr >= 0x4000 && addr <= 0x4017) {
    if (machine_) {
      machine_->apu().write_reg(addr, value);
    }
    return;
  }
  if (mapper_) {
    mapper_->cpu_write(addr, value);
  }
}

u8 SystemBus::peek(u16 addr) const {
  if (addr < 0x2000) {
    return ram_[addr & 0x07FF];
  }
  if (addr < 0x4000) {
    return machine_ ? machine_->ppu().reg_peek(addr) : 0;
  }
  if (addr == 0x4015) {
    return machine_ ? machine_->apu().read_status_peek() : 0;
  }
  if (addr == 0x4016) {
    return static_cast<u8>(0x40 | (shift_[0] & 1));
  }
  if (addr == 0x4017) {
    return static_cast<u8>(0x40 | (shift_[1] & 1));
  }
  if (addr < 0x4020) {
    return open_bus_;
  }
  if (mapper_) {
    return mapper_->cpu_peek(addr);
  }
  return open_bus_;
}

void SystemBus::set_controller_buttons(u8 player0, u8 player1) {
  buttons_[0] = player0;
  buttons_[1] = player1;
}

void SystemBus::write_controller_strobe(u8 value) {
  controller_strobe_ = static_cast<u8>(value & 1);
  if (controller_strobe_) {
    shift_[0] = buttons_[0];
    shift_[1] = buttons_[1];
  }
}

u8 SystemBus::read_controller(u8 player) {
  if (controller_strobe_) {
    shift_[player] = buttons_[player];
  }
  const u8 bit = static_cast<u8>(shift_[player] & 1);
  if (!controller_strobe_) {
    shift_[player] = static_cast<u8>((shift_[player] >> 1) | 0x80);
  }
  return static_cast<u8>(0x40 | bit);
}

void SystemBus::save_state(StateWriter& w) const {
  w.write_bytes(ram_.data(), ram_.size());
  w.write_u8(open_bus_);
  w.write_u8(controller_strobe_);
  w.write_u8(buttons_[0]);
  w.write_u8(buttons_[1]);
  w.write_u8(shift_[0]);
  w.write_u8(shift_[1]);
}

void SystemBus::load_state(StateReader& r) {
  r.read_bytes(ram_.data(), ram_.size());
  open_bus_ = r.read_u8();
  controller_strobe_ = r.read_u8();
  buttons_[0] = r.read_u8();
  buttons_[1] = r.read_u8();
  shift_[0] = r.read_u8();
  shift_[1] = r.read_u8();
}

}  // namespace nesemu
