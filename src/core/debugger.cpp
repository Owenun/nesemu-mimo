#include "nesemu/debugger.hpp"

#include "nesemu/machine.hpp"

#include <cstdio>

namespace nesemu {

void Debugger::add_breakpoint(u16 addr) {
  breakpoints_.push_back(Breakpoint{addr, true});
}

void Debugger::remove_breakpoint(std::size_t index) {
  if (index < breakpoints_.size()) {
    breakpoints_.erase(breakpoints_.begin() + static_cast<std::ptrdiff_t>(index));
  }
}

void Debugger::clear_breakpoints() {
  breakpoints_.clear();
}

void Debugger::add_watchpoint(u16 addr, bool on_read, bool on_write) {
  watchpoints_.push_back(Watchpoint{addr, on_read, on_write, true});
}

void Debugger::remove_watchpoint(std::size_t index) {
  if (index < watchpoints_.size()) {
    watchpoints_.erase(watchpoints_.begin() + static_cast<std::ptrdiff_t>(index));
  }
}

void Debugger::clear_watchpoints() {
  watchpoints_.clear();
}

void Debugger::request_pause(PauseReason reason) {
  paused_ = true;
  pause_reason_ = reason;
  step_budget_ = 0;
}

void Debugger::request_resume() {
  paused_ = false;
  pause_reason_ = PauseReason::None;
  step_budget_ = 0;
}

void Debugger::request_step_instruction() {
  paused_ = false;
  pause_reason_ = PauseReason::Step;
  step_mode_ = 0;
  step_budget_ = 1;
}

void Debugger::request_step_scanline() {
  paused_ = false;
  pause_reason_ = PauseReason::Step;
  step_mode_ = 1;
  step_budget_ = 1;
  if (machine_) {
    last_scanline_ = machine_->ppu().scanline();
  }
}

void Debugger::request_step_frame() {
  paused_ = false;
  pause_reason_ = PauseReason::Step;
  step_mode_ = 2;
  step_budget_ = 1;
  if (machine_) {
    last_frame_ = machine_->ppu().frame();
  }
}

void Debugger::on_bus_access(u16 addr, u8 value, bool is_write) {
  if (watchpoints_.empty() || paused_) {
    return;
  }
  for (const auto& wp : watchpoints_) {
    if (!wp.enabled || wp.addr != addr) {
      continue;
    }
    if ((is_write && wp.on_write) || (!is_write && wp.on_read)) {
      request_pause(PauseReason::Watchpoint);
      return;
    }
  }
  (void)value;
}

void Debugger::on_instruction_start(u16 pc, u8 a, u8 x, u8 y, u8 p, u8 sp, u64 cycles, u8 opcode) {
  for (const auto& bp : breakpoints_) {
    if (bp.enabled && bp.addr == pc) {
      request_pause(PauseReason::Breakpoint);
      return;
    }
  }

  TraceEntry e;
  e.pc = pc;
  e.a = a;
  e.x = x;
  e.y = y;
  e.p = p;
  e.sp = sp;
  e.cycles = cycles;
  e.opcode = opcode;
  if (recording_) {
    std::string dis;
    disassemble(pc, dis);
    e.disasm = dis;
    trace_.push_back(e);
    if (trace_.size() > trace_limit_) {
      trace_.pop_front();
    }
  }

  if (step_budget_ > 0) {
    --step_budget_;
    if (step_budget_ == 0) {
      // Will pause after this instruction completes — pause at next start.
      request_pause(PauseReason::Step);
    }
  }
}

std::size_t Debugger::disassemble(u16 addr, std::string& out) const {
  if (!machine_) {
    out = "???";
    return 1;
  }
  const SystemBus& bus = machine_->bus();
  const u8 op = bus.peek(addr);
  const u8 b1 = bus.peek(static_cast<u16>(addr + 1));
  const u8 b2 = bus.peek(static_cast<u16>(addr + 2));

  static const char* kNames[256] = {
      "BRK", "ORA", "???", "???", "NOP", "ORA", "ASL", "???", "PHP", "ORA", "ASL", "???",
      "NOP", "ORA", "ASL", "???", "BPL", "ORA", "???", "???", "NOP", "ORA", "ASL", "???",
      "CLC", "ORA", "NOP", "???", "NOP", "ORA", "ASL", "???", "JSR", "AND", "???", "???",
      "BIT", "AND", "ROL", "???", "PLP", "AND", "ROL", "???", "BIT", "AND", "ROL", "???",
      "BMI", "AND", "???", "???", "NOP", "AND", "ROL", "???", "SEC", "AND", "NOP", "???",
      "NOP", "AND", "ROL", "???", "RTI", "EOR", "???", "???", "NOP", "EOR", "LSR", "???",
      "PHA", "EOR", "LSR", "???", "JMP", "EOR", "LSR", "???", "BVC", "EOR", "???", "???",
      "NOP", "EOR", "LSR", "???", "CLI", "EOR", "NOP", "???", "NOP", "EOR", "LSR", "???",
      "RTS", "ADC", "???", "???", "NOP", "ADC", "ROR", "???", "PLA", "ADC", "ROR", "???",
      "JMP", "ADC", "ROR", "???", "BVS", "ADC", "???", "???", "NOP", "ADC", "ROR", "???",
      "SEI", "ADC", "NOP", "???", "NOP", "ADC", "ROR", "???", "STA", "STA", "NOP", "???",
      "STY", "STA", "STX", "???", "DEY", "NOP", "TXA", "???", "STY", "STA", "STX", "???",
      "BCC", "STA", "???", "???", "STY", "STA", "STX", "???", "TYA", "STA", "TXS", "???",
      "SHY", "STA", "SHX", "???", "LDY", "LDA", "LDX", "???", "LDY", "LDA", "LDX", "???",
      "TAY", "LDA", "TAX", "???", "LDY", "LDA", "LDX", "???", "BCS", "LDA", "???", "???",
      "LDY", "LDA", "LDX", "???", "CLV", "LDA", "TSX", "???", "LDY", "LDA", "LDX", "???",
      "CPY", "CMP", "NOP", "???", "CPY", "CMP", "DEC", "???", "INY", "CMP", "DEX", "???",
      "CPY", "CMP", "DEC", "???", "BNE", "CMP", "???", "???", "NOP", "CMP", "DEC", "???",
      "CLD", "CMP", "NOP", "???", "NOP", "CMP", "DEC", "???", "CPX", "SBC", "NOP", "???",
      "CPX", "SBC", "INC", "???", "INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "???",
      "BEQ", "SBC", "???", "???", "NOP", "SBC", "INC", "???", "SED", "SBC", "NOP", "???",
      "NOP", "SBC", "INC", "???",
  };

  // Very small size heuristic based on opcode low nibble groups (approximate).
  int size = 1;
  switch (op & 0x1F) {
    case 0x00:
    case 0x02:
    case 0x03:
    case 0x09:
    case 0x0B:
      size = (op == 0x00 || op == 0x40 || op == 0x60) ? 1 : ((op & 0x0F) == 0x09 || (op & 0x0F) == 0x0B ? 2 : 2);
      break;
    default:
      break;
  }
  // Better size table via simple patterns
  size = 1;
  const bool is_implied = (op & 0x0F) == 0x0A || op == 0xEA || (op & 0x0F) == 0x18 ||
                          (op & 0x0F) == 0x38 || (op & 0x0F) == 0x58 || (op & 0x0F) == 0x78 ||
                          (op & 0x0F) == 0x98 || (op & 0x0F) == 0xB8 || (op & 0x0F) == 0xD8 ||
                          (op & 0x0F) == 0xF8 || op == 0x08 || op == 0x28 || op == 0x48 ||
                          op == 0x68 || op == 0x40 || op == 0x60 || op == 0xAA || op == 0xA8 ||
                          op == 0xBA || op == 0x8A || op == 0x9A || op == 0xCA || op == 0x88 ||
                          op == 0xE8 || op == 0xC8;
  if (is_implied) {
    size = 1;
  } else if ((op & 0x1F) == 0x10 || (op & 0x0F) == 0x09 || (op & 0x0F) == 0x0B ||
             (op & 0x0F) == 0x01) {
    // relative / immediate / zp-ish ambiguous
    size = ((op & 0x1F) == 0x10) ? 2 : 2;
  } else if ((op & 0x1F) == 0x0C || (op & 0x1F) == 0x0E || (op & 0x1F) == 0x1E) {
    size = 3;
  } else if ((op & 0x0F) == 0x0C || (op & 0x0F) == 0x0E || (op & 0x0F) == 0x0D ||
             (op & 0x0F) == 0x0F) {
    size = 3;
  } else {
    size = 2;
  }
  // Force 3 for JMP/JSR/absolute families
  if (op == 0x20 || op == 0x4C || op == 0x6C) {
    size = 3;
  }

  char buf[64];
  if (size == 1) {
    std::snprintf(buf, sizeof(buf), "%s", kNames[op]);
  } else if (size == 2) {
    std::snprintf(buf, sizeof(buf), "%s $%02X", kNames[op], b1);
  } else {
    std::snprintf(buf, sizeof(buf), "%s $%02X%02X", kNames[op], b2, b1);
  }
  out = buf;
  return static_cast<std::size_t>(size);
}

void Debugger::save_state(StateWriter& w) const {
  w.write_bool(paused_);
  w.write_u8(static_cast<u8>(pause_reason_));
}

void Debugger::load_state(StateReader& r) {
  paused_ = r.read_bool();
  pause_reason_ = static_cast<PauseReason>(r.read_u8());
}

}  // namespace nesemu
