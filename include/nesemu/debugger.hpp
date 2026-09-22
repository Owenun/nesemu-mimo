#pragma once

#include "state.hpp"
#include "types.hpp"

#include <deque>
#include <string>
#include <vector>

namespace nesemu {

class Machine;

enum class PauseReason {
  None,
  User,
  Breakpoint,
  Watchpoint,
  Step,
  Jam,
  Frame,
};

struct Breakpoint {
  u16 addr = 0;
  bool enabled = true;
};

struct Watchpoint {
  u16 addr = 0;
  bool on_read = true;
  bool on_write = true;
  bool enabled = true;
};

struct TraceEntry {
  u16 pc = 0;
  u8 a = 0, x = 0, y = 0, p = 0, sp = 0;
  u64 cycles = 0;
  u8 opcode = 0;
  std::string disasm;
};

class Debugger {
public:
  void connect(Machine* machine) { machine_ = machine; }

  void add_breakpoint(u16 addr);
  void remove_breakpoint(std::size_t index);
  void clear_breakpoints();
  const std::vector<Breakpoint>& breakpoints() const { return breakpoints_; }

  void add_watchpoint(u16 addr, bool on_read, bool on_write);
  void remove_watchpoint(std::size_t index);
  void clear_watchpoints();
  const std::vector<Watchpoint>& watchpoints() const { return watchpoints_; }

  void request_pause(PauseReason reason = PauseReason::User);
  void request_resume();
  void request_step_instruction();
  void request_step_scanline();
  void request_step_frame();

  bool paused() const { return paused_; }
  PauseReason pause_reason() const { return pause_reason_; }

  // Called by Machine on each bus access (read/write only, not peek).
  void on_bus_access(u16 addr, u8 value, bool is_write);
  void on_instruction_start(u16 pc, u8 a, u8 x, u8 y, u8 p, u8 sp, u64 cycles, u8 opcode);

  bool wants_instruction_hook() const {
    // Only pay per-instruction cost when something is actually watching.
    return !breakpoints_.empty() || step_budget_ > 0 || recording_;
  }

  void set_recording(bool on) { recording_ = on; }

  // Disassemble one instruction at addr (uses peek). Returns size.
  std::size_t disassemble(u16 addr, std::string& out) const;
  std::deque<TraceEntry> trace() const { return trace_; }
  void clear_trace() { trace_.clear(); }

  void save_state(StateWriter& w) const;
  void load_state(StateReader& r);

private:
  Machine* machine_ = nullptr;
  std::vector<Breakpoint> breakpoints_;
  std::vector<Watchpoint> watchpoints_;
  std::deque<TraceEntry> trace_;
  std::size_t trace_limit_ = 4096;
  bool paused_ = false;
  PauseReason pause_reason_ = PauseReason::None;
  int step_budget_ = 0;  // instructions / scanlines / frames remaining
  int step_mode_ = 0;    // 0=instr 1=scanline 2=frame
  u16 last_scanline_ = 0;
  u64 last_frame_ = 0;
  bool recording_ = false;
};

}  // namespace nesemu
