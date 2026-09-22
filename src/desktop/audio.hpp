#pragma once

#include <vector>

namespace nesemu_desktop {

// Simple ring buffer consumed by SDL audio stream.
class AudioQueue {
public:
  void push(const float* samples, std::size_t count);
  std::size_t pop(float* out, std::size_t max_count);
  std::size_t size() const { return data_.size(); }
  void clear() { data_.clear(); }

private:
  std::vector<float> data_;
};

}  // namespace nesemu_desktop
