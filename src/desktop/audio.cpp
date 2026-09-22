#include "audio.hpp"

#include <algorithm>

namespace nesemu_desktop {

void AudioQueue::push(const float* samples, std::size_t count) {
  data_.insert(data_.end(), samples, samples + count);
  if (data_.size() > 44100) {
    data_.erase(data_.begin(), data_.begin() + static_cast<std::ptrdiff_t>(data_.size() - 44100));
  }
}

std::size_t AudioQueue::pop(float* out, std::size_t max_count) {
  const std::size_t n = std::min(max_count, data_.size());
  std::copy(data_.begin(), data_.begin() + static_cast<std::ptrdiff_t>(n), out);
  data_.erase(data_.begin(), data_.begin() + static_cast<std::ptrdiff_t>(n));
  return n;
}

}  // namespace nesemu_desktop
