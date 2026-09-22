#pragma once

#include "state.hpp"
#include "types.hpp"

#include <array>
#include <deque>

namespace nesemu {

class Apu {
public:
  void reset();

  void clock_cpu();  // one CPU cycle
  void write_reg(u16 addr, u8 value);
  u8 read_status();  // $4015
  u8 read_status_peek() const;

  // DMC DMA interface
  bool dmc_dma_pending() const { return dmc_dma_pending_; }
  void dmc_dma_complete(u8 sample_byte);
  void clear_dmc_dma() { dmc_dma_pending_ = false; }

  bool irq_line() const { return frame_irq_ || dmc_irq_; }

  // Samples produced at ~44100 Hz internally; host drains the ring.
  std::size_t samples_available() const;
  bool pop_sample(float& out);
  std::size_t pop_samples(float* out, std::size_t max_count);

  void save_state(StateWriter& w) const;
  void load_state(StateReader& r);

  // Debug
  float last_mixed() const { return last_mixed_; }

private:
  struct Envelope {
    bool start = false;
    bool loop = false;
    bool constant = false;
    u8 volume = 0;  // also divider reload
    u8 divider = 0;
    u8 decay = 0;

    void clock();
    u8 output() const { return constant ? volume : decay; }
  };

  struct Sweep {
    bool enabled = false;
    bool negate = false;
    bool reload = false;
    u8 period = 0;
    u8 shift = 0;
    u8 divider = 0;
    bool pulse1 = false;

    bool muted(u16 timer) const;
    u16 target(u16 timer) const;
    void clock(u16& timer);
  };

  struct Pulse {
    Envelope envelope;
    Sweep sweep;
    bool enabled = false;
    u8 duty = 0;
    u8 duty_pos = 0;
    bool length_halt = false;
    u8 length = 0;
    u16 timer = 0;
    u16 timer_period = 0;

    void clock_timer();
    void clock_length();
    void clock_quarter();
    u8 output() const;
    void write(u16 reg, u8 v);
  };

  struct Triangle {
    bool enabled = false;
    bool linear_reload = false;
    u8 linear_reload_value = 0;
    u8 linear_counter = 0;
    bool control = false;
    u8 length = 0;
    u16 timer = 0;
    u16 timer_period = 0;
    u8 seq_pos = 0;

    void clock_timer();
    void clock_length();
    void clock_quarter();
    u8 output() const;
  };

  struct Noise {
    Envelope envelope;
    bool enabled = false;
    bool mode = false;
    bool length_halt = false;
    u8 length = 0;
    u16 shift = 1;
    u16 timer = 0;
    u16 timer_period = 0;

    void clock_timer();
    void clock_length();
    void clock_quarter();
    u8 output() const;
  };

  struct Dmc {
    bool enabled = false;
    bool irq_enabled = false;
    bool loop = false;
    bool irq = false;
    u8 rate_index = 0;
    u8 output_level = 0;
    u8 bits_remaining = 0;
    u8 sample_buffer = 0;
    bool sample_buffer_empty = true;
    u8 shift = 0;
    u16 sample_address = 0;
    u16 sample_length = 0;
    u16 current_address = 0;
    u16 bytes_remaining = 0;
    u16 timer = 0;
    u16 timer_period = 0;
    bool dma_pending = false;

    void clock_timer();
    void start_dma_if_needed();
  };

  void clock_frame_counter();
  void clock_quarter_frame();
  void clock_half_frame();
  void mix_and_push_sample();

  Pulse pulse1_{};
  Pulse pulse2_{};
  Triangle triangle_{};
  Noise noise_{};
  Dmc dmc_{};

  // Frame counter
  bool five_step_ = false;
  bool irq_inhibit_ = false;
  bool frame_irq_ = false;
  bool dmc_irq_ = false;
  u32 frame_cycle_ = 0;
  u32 frame_reset_delay_ = 0;
  u8 frame_step_ = 0;

  // CPU cycle for DMC rate (CPU cycles)
  u32 cpu_cycle_ = 0;

  bool dmc_dma_pending_ = false;

  float last_mixed_ = 0.0f;
  std::size_t sample_head_ = 0;
  std::size_t sample_count_ = 0;
  static constexpr std::size_t kSampleCap = 8192;
  float sample_ring_[8192] = {};
  double sample_acc_ = 0.0;
  double cycles_per_sample_ = static_cast<double>(kCpuHz) / kApuSampleRate;

  static const u8 kLengthTable[32];
  static const u16 kNoisePeriod[16];
  static const u16 kDmcRate[16];
  static const u8 kDutyTable[4][8];
  static const u8 kTriangleTable[32];
};

}  // namespace nesemu
