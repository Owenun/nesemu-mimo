#pragma once

#include "apu.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "debugger.hpp"
#include "mapper.hpp"
#include "ppu.hpp"
#include "state.hpp"
#include "system_bus.hpp"
#include "types.hpp"

#include <memory>
#include <optional>
#include <string>

namespace nesemu {

class Machine {
public:
  static std::unique_ptr<Machine> load_rom(const ByteBuffer& rom, std::string* error = nullptr);

  void reset();
  void run_until_frame();
  void step_instruction();
  void step_scanline();
  void step_frame();

  // Timing hooks from SystemBus
  void on_cpu_bus_cycle();

  // Interrupt / DMA wiring
  u8 cpu_read_hook(u16 addr);
  void cpu_write_hook(u16 addr, u8 value);
  void tick_cpu_cycle();

  // OAM DMA ($4014)
  void request_oam_dma(u8 page);
  void request_dmc_dma();
  bool dmc_dma_pending() const;

  void set_controller_buttons(int player, const ControllerState& state);
  void set_controller_state(int player, u8 buttons);

  const Framebuffer& framebuffer() const { return ppu_.framebuffer(); }
  const Cartridge& cart() const { return *cart_; }
  Cartridge& cart() { return *cart_; }

  Cpu& cpu() { return cpu_; }
  const Cpu& cpu() const { return cpu_; }
  Ppu& ppu() { return ppu_; }
  const Ppu& ppu() const { return ppu_; }
  Apu& apu() { return apu_; }
  const Apu& apu() const { return apu_; }
  SystemBus& bus() { return bus_; }
  const SystemBus& bus() const { return bus_; }
  Mapper& mapper() { return *mapper_; }
  const Mapper& mapper() const { return *mapper_; }
  Debugger& debugger() { return debugger_; }
  const Debugger& debugger() const { return debugger_; }

  u64 cpu_cycles() const { return cpu_.cycles(); }
  u64 frame() const { return ppu_.frame(); }

  ByteBuffer save_state() const;
  bool load_state(const ByteBuffer& data, std::string* error = nullptr);

  u64 rom_fingerprint() const { return cart_ ? cart_->fingerprint() : 0; }

private:
  Machine() = default;

  void connect();
  void poll_interrupts();
  void service_oam_dma();
  void service_dmc_dma();
  void after_cpu_cycle();

  std::unique_ptr<Cartridge> cart_;
  std::unique_ptr<Mapper> mapper_;
  Cpu cpu_;
  Ppu ppu_;
  Apu apu_;
  SystemBus bus_;
  Debugger debugger_;

  // OAM DMA state
  bool oam_dma_pending_ = false;
  u8 oam_dma_page_ = 0;
  int oam_dma_cycles_ = 0;
  int oam_dma_index_ = 0;
  bool oam_dma_dummy_done_ = false;
  bool oam_dma_align_done_ = false;
  bool oam_dma_reading_ = false;

  // DMC DMA state
  int dmc_dma_cycles_ = 0;
  bool dmc_dma_read_done_ = false;

  bool frame_advanced_ = false;
  u16 last_scanline_ = 0;
  bool prev_nmi_line_ = false;
};

}  // namespace nesemu
