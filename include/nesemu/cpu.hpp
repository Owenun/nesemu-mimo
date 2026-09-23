#pragma once

#include "state.hpp"
#include "types.hpp"

namespace nesemu {

class CpuBus {
public:
  virtual ~CpuBus() = default;
  virtual u8 read(u16 addr) = 0;
  virtual void write(u16 addr, u8 value) = 0;
  virtual u8 peek(u16 addr) const = 0;
};

class Cpu {
public:
  enum Flag : u8 {
    C = 0x01,
    Z = 0x02,
    I = 0x04,
    D = 0x08,
    B = 0x10,
    U = 0x20,
    V = 0x40,
    N = 0x80,
  };

  void connect(CpuBus* bus) { bus_ = bus; }

  void reset();
  void nmi();
  void irq();
  void set_irq_line(bool level) { irq_line_ = level; }
  void step();

  bool jammed() const { return jammed_; }
  u64 cycles() const { return cycles_; }
  u64 nmi_count() const { return nmi_count_; }
  void set_cycles(u64 c) { cycles_ = c; }

  u8 a() const { return a_; }
  u8 x() const { return x_; }
  u8 y() const { return y_; }
  u8 sp() const { return sp_; }
  u8 p() const { return static_cast<u8>((p_ | U) & ~B); }
  u16 pc() const { return pc_; }

  void set_a(u8 v) { a_ = v; }
  void set_x(u8 v) { x_ = v; }
  void set_y(u8 v) { y_ = v; }
  void set_sp(u8 v) { sp_ = v; }
  void set_p(u8 v) { p_ = static_cast<u8>((v | U) & ~B); }
  void set_pc(u16 v) { pc_ = v; }

  bool flag(Flag f) const { return (p_ & f) != 0; }
  void set_flag(Flag f, bool on) {
    if (on) {
      p_ = static_cast<u8>(p_ | f);
    } else {
      p_ = static_cast<u8>(p_ & ~f);
    }
  }

  // B is not a real P bit; only appears when pushed to the stack.
  u8 p_pushed(bool brk) const {
    return static_cast<u8>((p_ | U) | (brk ? B : 0));
  }

  void save_state(StateWriter& w) const;
  void load_state(StateReader& r);

  enum class AddrMode {
    Implied,
    Accumulator,
    Immediate,
    ZeroPage,
    ZeroPageX,
    ZeroPageY,
    Relative,
    Absolute,
    AbsoluteX,
    AbsoluteY,
    Indirect,
    IndexedIndirect,
    IndirectIndexed,
  };

  enum class Op {
    ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS,
    CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX,
    INY, JMP, JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP,
    ROL, ROR, RTI, RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY,
    TSX, TXA, TXS, TYA,
    // Unofficial
    LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA, ANC, ALR, ARR, AXS, XAA,
    AHX, TAS, SHY, SHX, LAS, KIL,
  };

  struct OpInfo {
    Op op;
    AddrMode mode;
    bool unofficial;
  };

  static const OpInfo kTable[256];

private:
  u8 a_ = 0;
  u8 x_ = 0;
  u8 y_ = 0;
  u8 sp_ = 0xFD;
  u8 p_ = static_cast<u8>(U | I);
  u16 pc_ = 0;
  u64 cycles_ = 0;
  bool jammed_ = false;
  CpuBus* bus_ = nullptr;

  // Interrupt lines sampled just before each instruction fetch.
  bool nmi_pending_ = false;
  u64 nmi_count_ = 0;
  bool irq_line_ = false;

  u8 fetch8();
  u16 fetch16();
  u8 read8(u16 addr);
  void write8(u16 addr, u8 v);
  u16 read16(u16 addr);
  u16 read16_bug(u16 addr);

  void push8(u8 v);
  u8 pull8();
  void push16(u16 v);
  u16 pull16();

  void set_zn(u8 v);
  void branch(bool cond);
  void adc(u8 v);
  void sbc(u8 v);
  void compare(u8 reg, u8 v);

  // Returns effective address; may perform dummy reads on page cross.
  u16 ea(AddrMode mode, bool write_like = false);
  u8 read_operand(AddrMode mode);
  void write_operand(AddrMode mode, u8 value);
  void rmw(AddrMode mode, u8 (*fn)(Cpu&, u8));

  static u8 op_asl(Cpu& c, u8 v);
  static u8 op_lsr(Cpu& c, u8 v);
  static u8 op_rol(Cpu& c, u8 v);
  static u8 op_ror(Cpu& c, u8 v);
  static u8 op_inc(Cpu& c, u8 v);
  static u8 op_dec(Cpu& c, u8 v);

  void execute(Op op, AddrMode mode);
  void interrupt(u16 vector, bool brk);
};

}  // namespace nesemu
