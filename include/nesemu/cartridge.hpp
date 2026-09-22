#pragma once

#include "types.hpp"

#include <array>
#include <optional>
#include <string>

namespace nesemu {

class Cartridge {
public:
  static std::optional<Cartridge> from_bytes(const ByteBuffer& data, std::string* error = nullptr);

  const RomHeader& header() const { return header_; }
  const ByteBuffer& prg_rom() const { return prg_rom_; }
  const ByteBuffer& chr_rom() const { return chr_rom_; }
  ByteBuffer& chr_ram() { return chr_ram_; }
  const ByteBuffer& chr_ram() const { return chr_ram_; }
  ByteBuffer& prg_ram() { return prg_ram_; }
  const ByteBuffer& prg_ram() const { return prg_ram_; }
  const std::array<u8, 512>& trainer() const { return trainer_; }
  bool has_trainer() const { return header_.has_trainer; }

  bool has_battery() const { return header_.has_battery; }
  std::size_t battery_size() const { return prg_ram_.size(); }
  const u8* battery_data() const { return prg_ram_.empty() ? nullptr : prg_ram_.data(); }
  void battery_load(const u8* data, std::size_t size);

  u64 fingerprint() const { return fingerprint_; }
  u16 mapper_id() const { return header_.mapper_id; }
  Mirroring default_mirroring() const { return header_.mirroring; }

  bool uses_chr_ram() const { return chr_rom_.empty() && !chr_ram_.empty(); }

private:
  RomHeader header_{};
  ByteBuffer prg_rom_;
  ByteBuffer chr_rom_;
  ByteBuffer chr_ram_;
  ByteBuffer prg_ram_;
  std::array<u8, 512> trainer_{};
  u64 fingerprint_ = 0;
};

}  // namespace nesemu
