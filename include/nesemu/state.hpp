#pragma once

#include "types.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

namespace nesemu {

class StateError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class StateWriter {
public:
  void write_u8(u8 v) { data_.push_back(v); }
  void write_u16(u16 v) {
    data_.push_back(static_cast<u8>(v & 0xFF));
    data_.push_back(static_cast<u8>((v >> 8) & 0xFF));
  }
  void write_u32(u32 v) {
    for (int i = 0; i < 4; ++i) {
      data_.push_back(static_cast<u8>((v >> (8 * i)) & 0xFF));
    }
  }
  void write_u64(u64 v) {
    for (int i = 0; i < 8; ++i) {
      data_.push_back(static_cast<u8>((v >> (8 * i)) & 0xFF));
    }
  }
  void write_i8(i8 v) { write_u8(static_cast<u8>(v)); }
  void write_i16(i16 v) { write_u16(static_cast<u16>(v)); }
  void write_bool(bool v) { write_u8(v ? 1 : 0); }
  void write_bytes(const u8* data, std::size_t size) {
    data_.insert(data_.end(), data, data + size);
  }
  void write_bytes(const ByteBuffer& b) { write_bytes(b.data(), b.size()); }

  const ByteBuffer& data() const { return data_; }
  ByteBuffer& data() { return data_; }

private:
  ByteBuffer data_;
};

class StateReader {
public:
  explicit StateReader(const ByteBuffer& data) : data_(data) {}

  u8 read_u8() {
    need(1);
    return data_[pos_++];
  }
  u16 read_u16() {
    need(2);
    u16 v = static_cast<u16>(data_[pos_] | (data_[pos_ + 1] << 8));
    pos_ += 2;
    return v;
  }
  u32 read_u32() {
    need(4);
    u32 v = 0;
    for (int i = 0; i < 4; ++i) {
      v |= static_cast<u32>(data_[pos_ + static_cast<std::size_t>(i)]) << (8 * i);
    }
    pos_ += 4;
    return v;
  }
  u64 read_u64() {
    need(8);
    u64 v = 0;
    for (int i = 0; i < 8; ++i) {
      v |= static_cast<u64>(data_[pos_ + static_cast<std::size_t>(i)]) << (8 * i);
    }
    pos_ += 8;
    return v;
  }
  i8 read_i8() { return static_cast<i8>(read_u8()); }
  i16 read_i16() { return static_cast<i16>(read_u16()); }
  bool read_bool() { return read_u8() != 0; }
  void read_bytes(u8* out, std::size_t size) {
    need(size);
    std::memcpy(out, data_.data() + pos_, size);
    pos_ += size;
  }
  void read_bytes(ByteBuffer& out, std::size_t size) {
    out.resize(size);
    if (size > 0) {
      read_bytes(out.data(), size);
    }
  }

  std::size_t remaining() const { return data_.size() - pos_; }
  std::size_t pos() const { return pos_; }

private:
  void need(std::size_t n) {
    if (pos_ + n > data_.size()) {
      throw StateError("save state truncated");
    }
  }

  const ByteBuffer& data_;
  std::size_t pos_ = 0;
};

constexpr u32 kSaveStateMagic = 0x554D454Eu;  // "NEMU" little-endian
constexpr u32 kSaveStateVersion = 1u;

}  // namespace nesemu
