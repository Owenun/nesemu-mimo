#include "nesemu/cartridge.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  nesemu::ByteBuffer rom(data, data + size);
  std::string err;
  auto cart = nesemu::Cartridge::from_bytes(rom, &err);
  (void)cart;
  return 0;
}
