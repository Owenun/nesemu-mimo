#include "nesemu/state.hpp"

// State helpers live header-only; this TU keeps the library complete.

namespace nesemu {
namespace {
volatile int state_tu_anchor = 0;
}
int state_tu_anchor_ref() { return state_tu_anchor; }
}  // namespace nesemu
