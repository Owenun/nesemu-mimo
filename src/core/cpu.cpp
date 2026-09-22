#include "nesemu/cpu.hpp"

#include <cstdio>

namespace nesemu {

const Cpu::OpInfo Cpu::kTable[256] = {
    // 0x00
    {Cpu::Op::BRK, Cpu::AddrMode::Implied, false},
    {Cpu::Op::ORA, Cpu::AddrMode::IndexedIndirect, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::SLO, Cpu::AddrMode::IndexedIndirect, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::ORA, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::ASL, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::SLO, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::PHP, Cpu::AddrMode::Implied, false},
    {Cpu::Op::ORA, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::ASL, Cpu::AddrMode::Accumulator, false},
    {Cpu::Op::ANC, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::NOP, Cpu::AddrMode::Absolute, true},
    {Cpu::Op::ORA, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::ASL, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::SLO, Cpu::AddrMode::Absolute, true},
    // 0x10
    {Cpu::Op::BPL, Cpu::AddrMode::Relative, false},
    {Cpu::Op::ORA, Cpu::AddrMode::IndirectIndexed, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::SLO, Cpu::AddrMode::IndirectIndexed, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::ORA, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::ASL, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::SLO, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::CLC, Cpu::AddrMode::Implied, false},
    {Cpu::Op::ORA, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Implied, true},
    {Cpu::Op::SLO, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::NOP, Cpu::AddrMode::AbsoluteX, true},
    {Cpu::Op::ORA, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::ASL, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::SLO, Cpu::AddrMode::AbsoluteX, true},
    // 0x20
    {Cpu::Op::JSR, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::AND, Cpu::AddrMode::IndexedIndirect, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::RLA, Cpu::AddrMode::IndexedIndirect, true},
    {Cpu::Op::BIT, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::AND, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::ROL, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::RLA, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::PLP, Cpu::AddrMode::Implied, false},
    {Cpu::Op::AND, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::ROL, Cpu::AddrMode::Accumulator, false},
    {Cpu::Op::ANC, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::BIT, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::AND, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::ROL, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::RLA, Cpu::AddrMode::Absolute, true},
    // 0x30
    {Cpu::Op::BMI, Cpu::AddrMode::Relative, false},
    {Cpu::Op::AND, Cpu::AddrMode::IndirectIndexed, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::RLA, Cpu::AddrMode::IndirectIndexed, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::AND, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::ROL, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::RLA, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::SEC, Cpu::AddrMode::Implied, false},
    {Cpu::Op::AND, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Implied, true},
    {Cpu::Op::RLA, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::NOP, Cpu::AddrMode::AbsoluteX, true},
    {Cpu::Op::AND, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::ROL, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::RLA, Cpu::AddrMode::AbsoluteX, true},
    // 0x40
    {Cpu::Op::RTI, Cpu::AddrMode::Implied, false},
    {Cpu::Op::EOR, Cpu::AddrMode::IndexedIndirect, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::SRE, Cpu::AddrMode::IndexedIndirect, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::EOR, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::LSR, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::SRE, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::PHA, Cpu::AddrMode::Implied, false},
    {Cpu::Op::EOR, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::LSR, Cpu::AddrMode::Accumulator, false},
    {Cpu::Op::ALR, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::JMP, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::EOR, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::LSR, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::SRE, Cpu::AddrMode::Absolute, true},
    // 0x50
    {Cpu::Op::BVC, Cpu::AddrMode::Relative, false},
    {Cpu::Op::EOR, Cpu::AddrMode::IndirectIndexed, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::SRE, Cpu::AddrMode::IndirectIndexed, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::EOR, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::LSR, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::SRE, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::CLI, Cpu::AddrMode::Implied, false},
    {Cpu::Op::EOR, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Implied, true},
    {Cpu::Op::SRE, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::NOP, Cpu::AddrMode::AbsoluteX, true},
    {Cpu::Op::EOR, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::LSR, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::SRE, Cpu::AddrMode::AbsoluteX, true},
    // 0x60
    {Cpu::Op::RTS, Cpu::AddrMode::Implied, false},
    {Cpu::Op::ADC, Cpu::AddrMode::IndexedIndirect, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::RRA, Cpu::AddrMode::IndexedIndirect, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::ADC, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::ROR, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::RRA, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::PLA, Cpu::AddrMode::Implied, false},
    {Cpu::Op::ADC, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::ROR, Cpu::AddrMode::Accumulator, false},
    {Cpu::Op::ARR, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::JMP, Cpu::AddrMode::Indirect, false},
    {Cpu::Op::ADC, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::ROR, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::RRA, Cpu::AddrMode::Absolute, true},
    // 0x70
    {Cpu::Op::BVS, Cpu::AddrMode::Relative, false},
    {Cpu::Op::ADC, Cpu::AddrMode::IndirectIndexed, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::RRA, Cpu::AddrMode::IndirectIndexed, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::ADC, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::ROR, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::RRA, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::SEI, Cpu::AddrMode::Implied, false},
    {Cpu::Op::ADC, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Implied, true},
    {Cpu::Op::RRA, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::NOP, Cpu::AddrMode::AbsoluteX, true},
    {Cpu::Op::ADC, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::ROR, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::RRA, Cpu::AddrMode::AbsoluteX, true},
    // 0x80
    {Cpu::Op::NOP, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::STA, Cpu::AddrMode::IndexedIndirect, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::SAX, Cpu::AddrMode::IndexedIndirect, true},
    {Cpu::Op::STY, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::STA, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::STX, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::SAX, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::DEY, Cpu::AddrMode::Implied, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::TXA, Cpu::AddrMode::Implied, false},
    {Cpu::Op::XAA, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::STY, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::STA, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::STX, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::SAX, Cpu::AddrMode::Absolute, true},
    // 0x90
    {Cpu::Op::BCC, Cpu::AddrMode::Relative, false},
    {Cpu::Op::STA, Cpu::AddrMode::IndirectIndexed, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::AHX, Cpu::AddrMode::IndirectIndexed, true},
    {Cpu::Op::STY, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::STA, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::STX, Cpu::AddrMode::ZeroPageY, false},
    {Cpu::Op::SAX, Cpu::AddrMode::ZeroPageY, true},
    {Cpu::Op::TYA, Cpu::AddrMode::Implied, false},
    {Cpu::Op::STA, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::TXS, Cpu::AddrMode::Implied, false},
    {Cpu::Op::TAS, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::SHY, Cpu::AddrMode::AbsoluteX, true},
    {Cpu::Op::STA, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::SHX, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::AHX, Cpu::AddrMode::AbsoluteY, true},
    // 0xA0
    {Cpu::Op::LDY, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::LDA, Cpu::AddrMode::IndexedIndirect, false},
    {Cpu::Op::LDX, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::LAX, Cpu::AddrMode::IndexedIndirect, true},
    {Cpu::Op::LDY, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::LDA, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::LDX, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::LAX, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::TAY, Cpu::AddrMode::Implied, false},
    {Cpu::Op::LDA, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::TAX, Cpu::AddrMode::Implied, false},
    {Cpu::Op::LAX, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::LDY, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::LDA, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::LDX, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::LAX, Cpu::AddrMode::Absolute, true},
    // 0xB0
    {Cpu::Op::BCS, Cpu::AddrMode::Relative, false},
    {Cpu::Op::LDA, Cpu::AddrMode::IndirectIndexed, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::LAX, Cpu::AddrMode::IndirectIndexed, true},
    {Cpu::Op::LDY, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::LDA, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::LDX, Cpu::AddrMode::ZeroPageY, false},
    {Cpu::Op::LAX, Cpu::AddrMode::ZeroPageY, true},
    {Cpu::Op::CLV, Cpu::AddrMode::Implied, false},
    {Cpu::Op::LDA, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::TSX, Cpu::AddrMode::Implied, false},
    {Cpu::Op::LAS, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::LDY, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::LDA, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::LDX, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::LAX, Cpu::AddrMode::AbsoluteY, true},
    // 0xC0
    {Cpu::Op::CPY, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::CMP, Cpu::AddrMode::IndexedIndirect, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::DCP, Cpu::AddrMode::IndexedIndirect, true},
    {Cpu::Op::CPY, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::CMP, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::DEC, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::DCP, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::INY, Cpu::AddrMode::Implied, false},
    {Cpu::Op::CMP, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::DEX, Cpu::AddrMode::Implied, false},
    {Cpu::Op::AXS, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::CPY, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::CMP, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::DEC, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::DCP, Cpu::AddrMode::Absolute, true},
    // 0xD0
    {Cpu::Op::BNE, Cpu::AddrMode::Relative, false},
    {Cpu::Op::CMP, Cpu::AddrMode::IndirectIndexed, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::DCP, Cpu::AddrMode::IndirectIndexed, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::CMP, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::DEC, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::DCP, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::CLD, Cpu::AddrMode::Implied, false},
    {Cpu::Op::CMP, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Implied, true},
    {Cpu::Op::DCP, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::NOP, Cpu::AddrMode::AbsoluteX, true},
    {Cpu::Op::CMP, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::DEC, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::DCP, Cpu::AddrMode::AbsoluteX, true},
    // 0xE0
    {Cpu::Op::CPX, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::SBC, Cpu::AddrMode::IndexedIndirect, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Immediate, true},
    {Cpu::Op::ISC, Cpu::AddrMode::IndexedIndirect, true},
    {Cpu::Op::CPX, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::SBC, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::INC, Cpu::AddrMode::ZeroPage, false},
    {Cpu::Op::ISC, Cpu::AddrMode::ZeroPage, true},
    {Cpu::Op::INX, Cpu::AddrMode::Implied, false},
    {Cpu::Op::SBC, Cpu::AddrMode::Immediate, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Implied, false},  // 0xEA
    {Cpu::Op::SBC, Cpu::AddrMode::Immediate, true},  // 0xEB unofficial SBC
    {Cpu::Op::CPX, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::SBC, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::INC, Cpu::AddrMode::Absolute, false},
    {Cpu::Op::ISC, Cpu::AddrMode::Absolute, true},
    // 0xF0
    {Cpu::Op::BEQ, Cpu::AddrMode::Relative, false},
    {Cpu::Op::SBC, Cpu::AddrMode::IndirectIndexed, false},
    {Cpu::Op::KIL, Cpu::AddrMode::Implied, true},
    {Cpu::Op::ISC, Cpu::AddrMode::IndirectIndexed, true},
    {Cpu::Op::NOP, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::SBC, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::INC, Cpu::AddrMode::ZeroPageX, false},
    {Cpu::Op::ISC, Cpu::AddrMode::ZeroPageX, true},
    {Cpu::Op::SED, Cpu::AddrMode::Implied, false},
    {Cpu::Op::SBC, Cpu::AddrMode::AbsoluteY, false},
    {Cpu::Op::NOP, Cpu::AddrMode::Implied, true},
    {Cpu::Op::ISC, Cpu::AddrMode::AbsoluteY, true},
    {Cpu::Op::NOP, Cpu::AddrMode::AbsoluteX, true},
    {Cpu::Op::SBC, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::INC, Cpu::AddrMode::AbsoluteX, false},
    {Cpu::Op::ISC, Cpu::AddrMode::AbsoluteX, true},
};

void Cpu::reset() {
  a_ = 0;
  x_ = 0;
  y_ = 0;
  sp_ = 0xFD;
  p_ = static_cast<u8>(U | I);
  jammed_ = false;
  nmi_pending_ = false;
  irq_line_ = false;
  // Reset takes 7 cycles / 5 reads
  (void)read8(0xFFFC);
  (void)read8(0xFFFD);
  const u8 lo = read8(0xFFFC);
  const u8 hi = read8(0xFFFD);
  pc_ = static_cast<u16>(lo | (hi << 8));
  // Extra dummy cycles on reset (total 7)
  (void)read8(pc_);
  (void)read8(pc_);
}

void Cpu::nmi() { nmi_pending_ = true; }

void Cpu::irq() { irq_line_ = true; }

void Cpu::interrupt(u16 vector, bool brk) {
  (void)read8(pc_);  // dummy opcode fetch
  (void)read8(pc_);
  push16(pc_);
  push8(p_pushed(brk));
  set_flag(I, true);
  const u8 lo = read8(vector);
  const u8 hi = read8(static_cast<u16>(vector + 1));
  pc_ = static_cast<u16>(lo | (hi << 8));
}

void Cpu::step() {
  if (jammed_) {
    (void)read8(pc_);
    return;
  }

  // NMI is edge-latched; IRQ is level-sensitive and gated by I.
  // Do not permanently block IRQs here: handlers may return with RTS.
  if (nmi_pending_) {
    nmi_pending_ = false;
    interrupt(0xFFFA, false);
    return;
  }
  if (irq_line_ && !flag(I)) {
    interrupt(0xFFFE, false);
    return;
  }

  const u8 opcode = fetch8();
  const OpInfo info = kTable[opcode];
  execute(info.op, info.mode);
}

u8 Cpu::fetch8() {
  const u8 v = read8(pc_);
  pc_ = static_cast<u16>(pc_ + 1);
  return v;
}

u16 Cpu::fetch16() {
  const u8 lo = fetch8();
  const u8 hi = fetch8();
  return static_cast<u16>(lo | (hi << 8));
}

u8 Cpu::read8(u16 addr) { return bus_->read(addr); }

void Cpu::write8(u16 addr, u8 v) { bus_->write(addr, v); }

u16 Cpu::read16(u16 addr) {
  const u8 lo = read8(addr);
  const u8 hi = read8(static_cast<u16>(addr + 1));
  return static_cast<u16>(lo | (hi << 8));
}

u16 Cpu::read16_bug(u16 addr) {
  const u8 lo = read8(addr);
  const u16 hi_addr = static_cast<u16>((addr & 0xFF00) | ((addr + 1) & 0x00FF));
  const u8 hi = read8(hi_addr);
  return static_cast<u16>(lo | (hi << 8));
}

void Cpu::push8(u8 v) {
  write8(static_cast<u16>(0x0100 | sp_), v);
  sp_ = static_cast<u8>(sp_ - 1);
}

u8 Cpu::pull8() {
  sp_ = static_cast<u8>(sp_ + 1);
  return read8(static_cast<u16>(0x0100 | sp_));
}

void Cpu::push16(u16 v) {
  push8(static_cast<u8>((v >> 8) & 0xFF));
  push8(static_cast<u8>(v & 0xFF));
}

u16 Cpu::pull16() {
  const u8 lo = pull8();
  const u8 hi = pull8();
  return static_cast<u16>(lo | (hi << 8));
}

void Cpu::set_zn(u8 v) {
  set_flag(Z, v == 0);
  set_flag(N, (v & 0x80) != 0);
}

void Cpu::branch(bool cond) {
  const i8 off = static_cast<i8>(fetch8());
  if (!cond) {
    return;
  }
  const u16 old = pc_;
  pc_ = static_cast<u16>(pc_ + off);
  (void)read8(old);  // dummy fetch of next opcode
  if ((old & 0xFF00) != (pc_ & 0xFF00)) {
    (void)read8(static_cast<u16>((old & 0xFF00) | (pc_ & 0x00FF)));
  }
}

void Cpu::adc(u8 v) {
  const u16 sum = static_cast<u16>(a_) + v + (flag(C) ? 1 : 0);
  set_flag(C, sum > 0xFF);
  set_flag(V, (~(a_ ^ v) & (a_ ^ static_cast<u8>(sum)) & 0x80) != 0);
  a_ = static_cast<u8>(sum & 0xFF);
  set_zn(a_);
}

void Cpu::sbc(u8 v) {
  adc(static_cast<u8>(~v));
}

void Cpu::compare(u8 reg, u8 v) {
  const u16 t = static_cast<u16>(reg) - v;
  set_flag(C, reg >= v);
  set_zn(static_cast<u8>(t & 0xFF));
}

u16 Cpu::ea(AddrMode mode, bool write_like) {
  switch (mode) {
    case AddrMode::Implied:
    case AddrMode::Accumulator:
      return 0;
    case AddrMode::Immediate:
      return pc_++;
    case AddrMode::ZeroPage: {
      return fetch8();
    }
    case AddrMode::ZeroPageX: {
      const u8 zp = fetch8();
      (void)read8(zp);  // dummy
      return static_cast<u8>(zp + x_);
    }
    case AddrMode::ZeroPageY: {
      const u8 zp = fetch8();
      (void)read8(zp);
      return static_cast<u8>(zp + y_);
    }
    case AddrMode::Relative:
      return 0;
    case AddrMode::Absolute:
      return fetch16();
    case AddrMode::AbsoluteX: {
      const u16 base = fetch16();
      const u16 addr = static_cast<u16>(base + x_);
      if (write_like || (base & 0xFF00) != (addr & 0xFF00)) {
        (void)read8(static_cast<u16>((base & 0xFF00) | (addr & 0x00FF)));
      }
      return addr;
    }
    case AddrMode::AbsoluteY: {
      const u16 base = fetch16();
      const u16 addr = static_cast<u16>(base + y_);
      if (write_like || (base & 0xFF00) != (addr & 0xFF00)) {
        (void)read8(static_cast<u16>((base & 0xFF00) | (addr & 0x00FF)));
      }
      return addr;
    }
    case AddrMode::Indirect: {
      const u16 ptr = fetch16();
      return read16_bug(ptr);
    }
    case AddrMode::IndexedIndirect: {
      const u8 zp = fetch8();
      (void)read8(zp);
      return read16_bug(static_cast<u8>(zp + x_));
    }
    case AddrMode::IndirectIndexed: {
      const u8 zp = fetch8();
      const u16 base = read16_bug(zp);
      const u16 addr = static_cast<u16>(base + y_);
      if (write_like || (base & 0xFF00) != (addr & 0xFF00)) {
        (void)read8(static_cast<u16>((base & 0xFF00) | (addr & 0x00FF)));
      }
      return addr;
    }
  }
  return 0;
}

u8 Cpu::read_operand(AddrMode mode) {
  if (mode == AddrMode::Accumulator) {
    (void)read8(pc_);  // dummy cycle
    return a_;
  }
  if (mode == AddrMode::Implied) {
    (void)read8(pc_);  // dummy cycle
    return 0;
  }
  return read8(ea(mode, false));
}

void Cpu::write_operand(AddrMode mode, u8 value) {
  if (mode == AddrMode::Accumulator) {
    (void)read8(pc_);
    a_ = value;
    return;
  }
  write8(ea(mode, true), value);
}

void Cpu::rmw(AddrMode mode, u8 (*fn)(Cpu&, u8)) {
  if (mode == AddrMode::Accumulator) {
    (void)read8(pc_);
    a_ = fn(*this, a_);
    set_zn(a_);
    return;
  }
  const u16 addr = ea(mode, true);
  const u8 old = read8(addr);
  write8(addr, old);  // dummy write of original value
  const u8 neu = fn(*this, old);
  write8(addr, neu);
  set_zn(neu);
}

u8 Cpu::op_asl(Cpu& c, u8 v) {
  c.set_flag(C, (v & 0x80) != 0);
  return static_cast<u8>(v << 1);
}

u8 Cpu::op_lsr(Cpu& c, u8 v) {
  c.set_flag(C, (v & 0x01) != 0);
  return static_cast<u8>(v >> 1);
}

u8 Cpu::op_rol(Cpu& c, u8 v) {
  const u8 carry = c.flag(C) ? 1 : 0;
  c.set_flag(C, (v & 0x80) != 0);
  return static_cast<u8>((v << 1) | carry);
}

u8 Cpu::op_ror(Cpu& c, u8 v) {
  const u8 carry = c.flag(C) ? 0x80 : 0;
  c.set_flag(C, (v & 0x01) != 0);
  return static_cast<u8>((v >> 1) | carry);
}

u8 Cpu::op_inc(Cpu&, u8 v) { return static_cast<u8>(v + 1); }

u8 Cpu::op_dec(Cpu&, u8 v) { return static_cast<u8>(v - 1); }

void Cpu::execute(Op op, AddrMode mode) {
  // Implied / Accumulator ops need one dummy bus cycle after the opcode fetch.
  auto implied_touch = [&]() {
    if (mode == AddrMode::Implied || mode == AddrMode::Accumulator) {
      (void)read8(pc_);
    }
  };

  switch (op) {
    case Op::ADC:
      adc(read_operand(mode));
      break;
    case Op::AND:
      a_ = static_cast<u8>(a_ & read_operand(mode));
      set_zn(a_);
      break;
    case Op::ASL:
      rmw(mode, op_asl);
      break;
    case Op::BCC:
      branch(!flag(C));
      break;
    case Op::BCS:
      branch(flag(C));
      break;
    case Op::BEQ:
      branch(flag(Z));
      break;
    case Op::BIT: {
      const u8 v = read_operand(mode);
      set_flag(Z, (a_ & v) == 0);
      set_flag(V, (v & 0x40) != 0);
      set_flag(N, (v & 0x80) != 0);
      break;
    }
    case Op::BMI:
      branch(flag(N));
      break;
    case Op::BNE:
      branch(!flag(Z));
      break;
    case Op::BPL:
      branch(!flag(N));
      break;
    case Op::BRK:
      pc_ = static_cast<u16>(pc_ + 1);  // BRK padding byte
      interrupt(0xFFFE, true);
      break;
    case Op::BVC:
      branch(!flag(V));
      break;
    case Op::BVS:
      branch(flag(V));
      break;
    case Op::CLC:
      implied_touch();
      set_flag(C, false);
      break;
    case Op::CLD:
      implied_touch();
      set_flag(D, false);
      break;
    case Op::CLI:
      implied_touch();
      set_flag(I, false);
      break;
    case Op::CLV:
      implied_touch();
      set_flag(V, false);
      break;
    case Op::CMP:
      compare(a_, read_operand(mode));
      break;
    case Op::CPX:
      compare(x_, read_operand(mode));
      break;
    case Op::CPY:
      compare(y_, read_operand(mode));
      break;
    case Op::DEC:
      rmw(mode, op_dec);
      break;
    case Op::DEX:
      implied_touch();
      x_ = static_cast<u8>(x_ - 1);
      set_zn(x_);
      break;
    case Op::DEY:
      implied_touch();
      y_ = static_cast<u8>(y_ - 1);
      set_zn(y_);
      break;
    case Op::EOR:
      a_ = static_cast<u8>(a_ ^ read_operand(mode));
      set_zn(a_);
      break;
    case Op::INC:
      rmw(mode, op_inc);
      break;
    case Op::INX:
      implied_touch();
      x_ = static_cast<u8>(x_ + 1);
      set_zn(x_);
      break;
    case Op::INY:
      implied_touch();
      y_ = static_cast<u8>(y_ + 1);
      set_zn(y_);
      break;
    case Op::JMP:
      pc_ = ea(mode, true);
      break;
    case Op::JSR: {
      const u16 target = fetch16();
      (void)read8(static_cast<u16>(0x0100 | sp_));
      push16(static_cast<u16>(pc_ - 1));
      pc_ = target;
      break;
    }
    case Op::LDA:
      a_ = read_operand(mode);
      set_zn(a_);
      break;
    case Op::LDX:
      x_ = read_operand(mode);
      set_zn(x_);
      break;
    case Op::LDY:
      y_ = read_operand(mode);
      set_zn(y_);
      break;
    case Op::LSR:
      rmw(mode, op_lsr);
      break;
    case Op::NOP:
      if (mode != AddrMode::Implied && mode != AddrMode::Accumulator) {
        (void)read_operand(mode);
      } else {
        implied_touch();
      }
      break;
    case Op::ORA:
      a_ = static_cast<u8>(a_ | read_operand(mode));
      set_zn(a_);
      break;
    case Op::PHA:
      (void)read8(pc_);
      push8(a_);
      break;
    case Op::PHP: {
      (void)read8(pc_);
      push8(p_pushed(true));
      break;
    }
    case Op::PLA: {
      (void)read8(pc_);
      (void)read8(static_cast<u16>(0x0100 | sp_));
      a_ = pull8();
      set_zn(a_);
      break;
    }
    case Op::PLP: {
      (void)read8(pc_);
      (void)read8(static_cast<u16>(0x0100 | sp_));
      set_p(pull8());
      break;
    }
    case Op::ROL:
      rmw(mode, op_rol);
      break;
    case Op::ROR:
      rmw(mode, op_ror);
      break;
    case Op::RTI: {
      (void)read8(pc_);
      (void)read8(static_cast<u16>(0x0100 | sp_));
      set_p(pull8());
      pc_ = pull16();
      break;
    }
    case Op::RTS: {
      (void)read8(pc_);
      (void)read8(static_cast<u16>(0x0100 | sp_));
      pc_ = pull16();
      (void)read8(pc_);
      pc_ = static_cast<u16>(pc_ + 1);
      break;
    }
    case Op::SBC:
      sbc(read_operand(mode));
      break;
    case Op::SEC:
      implied_touch();
      set_flag(C, true);
      break;
    case Op::SED:
      implied_touch();
      set_flag(D, true);
      break;
    case Op::SEI:
      implied_touch();
      set_flag(I, true);
      break;
    case Op::STA:
      write_operand(mode, a_);
      break;
    case Op::STX:
      write_operand(mode, x_);
      break;
    case Op::STY:
      write_operand(mode, y_);
      break;
    case Op::TAX:
      implied_touch();
      x_ = a_;
      set_zn(x_);
      break;
    case Op::TAY:
      implied_touch();
      y_ = a_;
      set_zn(y_);
      break;
    case Op::TSX:
      implied_touch();
      x_ = sp_;
      set_zn(x_);
      break;
    case Op::TXA:
      implied_touch();
      a_ = x_;
      set_zn(a_);
      break;
    case Op::TXS:
      implied_touch();
      sp_ = x_;
      break;
    case Op::TYA:
      implied_touch();
      a_ = y_;
      set_zn(a_);
      break;

    case Op::LAX: {
      const u8 v = read_operand(mode);
      a_ = v;
      x_ = v;
      set_zn(v);
      break;
    }
    case Op::SAX: {
      write_operand(mode, static_cast<u8>(a_ & x_));
      break;
    }
    case Op::DCP: {
      const u16 addr = ea(mode, true);
      const u8 old = read8(addr);
      write8(addr, old);
      const u8 neu = static_cast<u8>(old - 1);
      write8(addr, neu);
      compare(a_, neu);
      break;
    }
    case Op::ISC: {
      const u16 addr = ea(mode, true);
      const u8 old = read8(addr);
      write8(addr, old);
      const u8 neu = static_cast<u8>(old + 1);
      write8(addr, neu);
      sbc(neu);
      break;
    }
    case Op::SLO: {
      const u16 addr = ea(mode, true);
      const u8 old = read8(addr);
      write8(addr, old);
      const u8 neu = op_asl(*this, old);
      write8(addr, neu);
      set_zn(neu);
      a_ = static_cast<u8>(a_ | neu);
      set_zn(a_);
      break;
    }
    case Op::RLA: {
      const u16 addr = ea(mode, true);
      const u8 old = read8(addr);
      write8(addr, old);
      const u8 neu = op_rol(*this, old);
      write8(addr, neu);
      set_zn(neu);
      a_ = static_cast<u8>(a_ & neu);
      set_zn(a_);
      break;
    }
    case Op::SRE: {
      const u16 addr = ea(mode, true);
      const u8 old = read8(addr);
      write8(addr, old);
      const u8 neu = op_lsr(*this, old);
      write8(addr, neu);
      set_zn(neu);
      a_ = static_cast<u8>(a_ ^ neu);
      set_zn(a_);
      break;
    }
    case Op::RRA: {
      const u16 addr = ea(mode, true);
      const u8 old = read8(addr);
      write8(addr, old);
      const u8 neu = op_ror(*this, old);
      write8(addr, neu);
      set_zn(neu);
      adc(neu);
      break;
    }
    case Op::ANC: {
      a_ = static_cast<u8>(a_ & read_operand(mode));
      set_zn(a_);
      set_flag(C, (a_ & 0x80) != 0);
      break;
    }
    case Op::ALR: {
      a_ = static_cast<u8>(a_ & read_operand(mode));
      a_ = op_lsr(*this, a_);
      set_zn(a_);
      break;
    }
    case Op::ARR: {
      u8 v = static_cast<u8>(a_ & read_operand(mode));
      const u8 carry = flag(C) ? 0x80 : 0;
      v = static_cast<u8>((v >> 1) | carry);
      a_ = v;
      set_zn(a_);
      set_flag(C, (v & 0x40) != 0);
      set_flag(V, ((v >> 6) ^ (v >> 5)) & 1);
      break;
    }
    case Op::AXS: {
      const u8 v = read_operand(mode);
      const u8 t = static_cast<u8>(a_ & x_);
      set_flag(C, t >= v);
      x_ = static_cast<u8>(t - v);
      set_zn(x_);
      break;
    }
    case Op::XAA: {
      // Unstable: approximated as A = X & imm
      a_ = static_cast<u8>(x_ & read_operand(mode));
      set_zn(a_);
      break;
    }
    case Op::AHX: {
      const u16 addr = ea(mode, true);
      const u8 h = static_cast<u8>(a_ & x_ & static_cast<u8>((addr >> 8) + 1));
      write8(addr, h);
      break;
    }
    case Op::TAS: {
      const u16 addr = ea(mode, true);
      sp_ = static_cast<u8>(a_ & x_);
      const u8 h = static_cast<u8>(sp_ & static_cast<u8>((addr >> 8) + 1));
      write8(addr, h);
      break;
    }
    case Op::SHY: {
      const u16 addr = ea(mode, true);
      const u8 h = static_cast<u8>(y_ & static_cast<u8>((addr >> 8) + 1));
      write8(addr, h);
      break;
    }
    case Op::SHX: {
      const u16 addr = ea(mode, true);
      const u8 h = static_cast<u8>(x_ & static_cast<u8>((addr >> 8) + 1));
      write8(addr, h);
      break;
    }
    case Op::LAS: {
      const u8 v = static_cast<u8>(read_operand(mode) & sp_);
      a_ = v;
      x_ = v;
      sp_ = v;
      set_zn(v);
      break;
    }
    case Op::KIL:
      jammed_ = true;
      break;
  }
}

void Cpu::save_state(StateWriter& w) const {
  w.write_u8(a_);
  w.write_u8(x_);
  w.write_u8(y_);
  w.write_u8(sp_);
  w.write_u8(p_);
  w.write_u16(pc_);
  w.write_u64(cycles_);
  w.write_bool(jammed_);
  w.write_bool(nmi_pending_);
  w.write_bool(irq_line_);
}

void Cpu::load_state(StateReader& r) {
  a_ = r.read_u8();
  x_ = r.read_u8();
  y_ = r.read_u8();
  sp_ = r.read_u8();
  p_ = static_cast<u8>((r.read_u8() | U) & ~B);
  pc_ = r.read_u16();
  cycles_ = r.read_u64();
  jammed_ = r.read_bool();
  nmi_pending_ = r.read_bool();
  irq_line_ = r.read_bool();
}

}  // namespace nesemu
