#include "nesemu/controller.hpp"

namespace nesemu {

void Controller::write_strobe(u8 value) {
  strobe_ = static_cast<u8>(value & 1);
  if (strobe_) {
    shift_ = buttons_;
  }
}

u8 Controller::read() {
  if (strobe_) {
    shift_ = buttons_;
  }
  const u8 bit = static_cast<u8>(shift_ & 1);
  if (!strobe_) {
    shift_ = static_cast<u8>((shift_ >> 1) | 0x80);
  }
  return bit;
}

void Controller::save_state(StateWriter& w) const {
  w.write_u8(buttons_);
  w.write_u8(shift_);
  w.write_u8(strobe_);
}

void Controller::load_state(StateReader& r) {
  buttons_ = r.read_u8();
  shift_ = r.read_u8();
  strobe_ = r.read_u8();
}

}  // namespace nesemu
