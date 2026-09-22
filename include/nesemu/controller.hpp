#pragma once

#include "state.hpp"
#include "types.hpp"

namespace nesemu {

// Serial NES controller helper (optional; SystemBus owns the live protocol).
class Controller {
public:
  void set_buttons(u8 buttons) { buttons_ = buttons; }
  u8 buttons() const { return buttons_; }

  void write_strobe(u8 value);
  u8 read();

  void save_state(StateWriter& w) const;
  void load_state(StateReader& r);

private:
  u8 buttons_ = 0;
  u8 shift_ = 0;
  u8 strobe_ = 0;
};

}  // namespace nesemu
