#include "nesemu/mapper.hpp"

#include <memory>

namespace nesemu {

std::unique_ptr<Mapper> make_mapper000(Cartridge& c);
std::unique_ptr<Mapper> make_mapper001(Cartridge& c);
std::unique_ptr<Mapper> make_mapper002(Cartridge& c);
std::unique_ptr<Mapper> make_mapper003(Cartridge& c);
std::unique_ptr<Mapper> make_mapper004(Cartridge& c);
std::unique_ptr<Mapper> make_mapper007(Cartridge& c);

Mirroring Mapper::mirroring() const {
  return cart_.default_mirroring();
}

u8 Mapper::prg_read(std::size_t index) const {
  const auto& prg = cart_.prg_rom();
  if (prg.empty()) {
    return 0;
  }
  return prg[index % prg.size()];
}

u8 Mapper::chr_read(std::size_t index) const {
  if (!cart_.chr_rom().empty()) {
    const auto& chr = cart_.chr_rom();
    return chr[index % chr.size()];
  }
  const auto& chr = cart_.chr_ram();
  if (chr.empty()) {
    return 0;
  }
  return chr[index % chr.size()];
}

void Mapper::chr_write(std::size_t index, u8 value) {
  if (cart_.chr_rom().empty()) {
    auto& chr = cart_.chr_ram();
    if (!chr.empty()) {
      chr[index % chr.size()] = value;
    }
  }
}

u8 Mapper::prg_ram_read(std::size_t index) const {
  const auto& ram = cart_.prg_ram();
  if (ram.empty()) {
    return 0;
  }
  return ram[index % ram.size()];
}

void Mapper::prg_ram_write(std::size_t index, u8 value) {
  auto& ram = cart_.prg_ram();
  if (!ram.empty()) {
    ram[index % ram.size()] = value;
  }
}

std::unique_ptr<Mapper> Mapper::create(Cartridge& cart, std::string* error) {
  switch (cart.mapper_id()) {
    case 0:
      return make_mapper000(cart);
    case 1:
      return make_mapper001(cart);
    case 2:
      return make_mapper002(cart);
    case 3:
      return make_mapper003(cart);
    case 4:
      return make_mapper004(cart);
    case 7:
      return make_mapper007(cart);
    default:
      if (error) {
        *error = "unsupported mapper " + std::to_string(cart.mapper_id());
      }
      return nullptr;
  }
}

}  // namespace nesemu
