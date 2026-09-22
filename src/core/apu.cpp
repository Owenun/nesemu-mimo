#include "nesemu/apu.hpp"

#include <cmath>

namespace nesemu {

const u8 Apu::kLengthTable[32] = {
    10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
    12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

const u16 Apu::kNoisePeriod[16] = {4, 8, 16, 32, 64, 96, 128, 160,
                                   202, 254, 380, 508, 762, 1016, 2034, 4068};

const u16 Apu::kDmcRate[16] = {428, 380, 340, 320, 286, 254, 226, 214,
                               190, 160, 142, 128, 106, 84, 72, 54};

const u8 Apu::kDutyTable[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1},
};

const u8 Apu::kTriangleTable[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,  4,  3,  2,  1,  0,
    0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

void Apu::Envelope::clock() {
  if (start) {
    start = false;
    decay = 15;
    divider = volume;
    return;
  }
  if (divider == 0) {
    divider = volume;
    if (decay > 0) {
      --decay;
    } else if (loop) {
      decay = 15;
    }
  } else {
    --divider;
  }
}

bool Apu::Sweep::muted(u16 timer) const {
  return timer < 8 || target(timer) > 0x7FF;
}

u16 Apu::Sweep::target(u16 timer) const {
  const i16 change = static_cast<i16>(timer >> shift);
  if (negate) {
    if (pulse1) {
      return static_cast<u16>(timer - change - 1);
    }
    return static_cast<u16>(timer - change);
  }
  return static_cast<u16>(timer + change);
}

void Apu::Sweep::clock(u16& timer) {
  if (divider == 0 && enabled && shift != 0 && !muted(timer)) {
    timer = target(timer);
  }
  if (divider == 0 || reload) {
    divider = period;
    reload = false;
  } else {
    --divider;
  }
}

void Apu::Pulse::clock_timer() {
  if (timer == 0) {
    timer = timer_period;
    duty_pos = static_cast<u8>((duty_pos + 1) & 7);
  } else {
    --timer;
  }
}

void Apu::Pulse::clock_length() {
  if (!length_halt && length > 0) {
    --length;
  }
}

void Apu::Pulse::clock_quarter() {
  envelope.clock();
}

u8 Apu::Pulse::output() const {
  if (!enabled || length == 0 || sweep.muted(timer_period)) {
    return 0;
  }
  if (kDutyTable[duty & 3][duty_pos] == 0) {
    return 0;
  }
  return envelope.output();
}

void Apu::Pulse::write(u16 reg, u8 v) {
  switch (reg) {
    case 0:
      duty = static_cast<u8>(v >> 6);
      length_halt = (v & 0x20) != 0;
      envelope.loop = length_halt;
      envelope.constant = (v & 0x10) != 0;
      envelope.volume = static_cast<u8>(v & 0x0F);
      break;
    case 1:
      sweep.enabled = (v & 0x80) != 0;
      sweep.period = static_cast<u8>((v >> 4) & 0x07);
      sweep.negate = (v & 0x08) != 0;
      sweep.shift = static_cast<u8>(v & 0x07);
      sweep.reload = true;
      break;
    case 2:
      timer_period = static_cast<u16>((timer_period & 0x700) | v);
      break;
    case 3:
      timer_period = static_cast<u16>((timer_period & 0xFF) | ((v & 0x07) << 8));
      if (enabled) {
        length = kLengthTable[(v >> 3) & 0x1F];
      }
      duty_pos = 0;
      envelope.start = true;
      break;
    default:
      break;
  }
}

void Apu::Triangle::clock_timer() {
  if (timer == 0) {
    timer = timer_period;
    if (length > 0 && linear_counter > 0 && timer_period > 1) {
      seq_pos = static_cast<u8>((seq_pos + 1) & 31);
    }
  } else {
    --timer;
  }
}

void Apu::Triangle::clock_length() {
  if (!control && length > 0) {
    --length;
  }
}

void Apu::Triangle::clock_quarter() {
  if (linear_reload) {
    linear_counter = linear_reload_value;
  } else if (linear_counter > 0) {
    --linear_counter;
  }
  if (!control) {
    linear_reload = false;
  }
}

u8 Apu::Triangle::output() const {
  if (!enabled) {
    return 0;
  }
  return kTriangleTable[seq_pos];
}

void Apu::Noise::clock_timer() {
  if (timer == 0) {
    timer = timer_period;
    const u8 bit = mode ? static_cast<u8>((shift >> 6) & 1) : static_cast<u8>((shift >> 1) & 1);
    const u8 feedback = static_cast<u8>((shift & 1) ^ bit);
    shift = static_cast<u16>((shift >> 1) | (static_cast<u16>(feedback) << 14));
  } else {
    --timer;
  }
}

void Apu::Noise::clock_length() {
  if (!length_halt && length > 0) {
    --length;
  }
}

void Apu::Noise::clock_quarter() {
  envelope.clock();
}

u8 Apu::Noise::output() const {
  if (!enabled || length == 0 || (shift & 1)) {
    return 0;
  }
  return envelope.output();
}

void Apu::Dmc::clock_timer() {
  if (timer == 0) {
    timer = timer_period;
    // Output unit
    if ((shift & 1) != 0) {
      if (output_level <= 125) {
        output_level = static_cast<u8>(output_level + 2);
      }
    } else {
      if (output_level >= 2) {
        output_level = static_cast<u8>(output_level - 2);
      }
    }
    shift = static_cast<u8>(shift >> 1);
    --bits_remaining;
    if (bits_remaining == 0) {
      bits_remaining = 8;
      if (sample_buffer_empty) {
        // silence
      } else {
        shift = sample_buffer;
        sample_buffer_empty = true;
        dma_pending = true;
      }
    }
    if (bytes_remaining == 0 && loop) {
      // restart handled by start_dma / machine refill
    }
    if (bytes_remaining == 0 && !loop) {
      if (irq_enabled) {
        irq = true;
      }
    }
  } else {
    --timer;
  }
}

void Apu::Dmc::start_dma_if_needed() {
  if (bytes_remaining > 0 && sample_buffer_empty) {
    dma_pending = true;
  }
}

void Apu::reset() {
  *this = Apu();
  sample_head_ = 0; sample_count_ = 0;
  pulse1_.sweep.pulse1 = true;
  pulse2_.sweep.pulse1 = false;
  noise_.shift = 1;
  cycles_per_sample_ = static_cast<double>(kCpuHz) / kApuSampleRate;
}

void Apu::write_reg(u16 addr, u8 value) {
  switch (addr) {
    case 0x4000:
    case 0x4001:
    case 0x4002:
    case 0x4003:
      pulse1_.write(static_cast<u16>(addr & 3), value);
      break;
    case 0x4004:
    case 0x4005:
    case 0x4006:
    case 0x4007:
      pulse2_.write(static_cast<u16>(addr & 3), value);
      break;
    case 0x4008:
      triangle_.control = (value & 0x80) != 0;
      triangle_.linear_reload_value = static_cast<u8>(value & 0x7F);
      break;
    case 0x400A:
      triangle_.timer_period = static_cast<u16>((triangle_.timer_period & 0x700) | value);
      break;
    case 0x400B:
      triangle_.timer_period =
          static_cast<u16>((triangle_.timer_period & 0xFF) | ((value & 0x07) << 8));
      if (triangle_.enabled) {
        triangle_.length = kLengthTable[(value >> 3) & 0x1F];
      }
      triangle_.linear_reload = true;
      break;
    case 0x400C:
      noise_.envelope.loop = (value & 0x20) != 0;
      noise_.length_halt = noise_.envelope.loop;
      noise_.envelope.constant = (value & 0x10) != 0;
      noise_.envelope.volume = static_cast<u8>(value & 0x0F);
      break;
    case 0x400E:
      noise_.mode = (value & 0x80) != 0;
      noise_.timer_period = kNoisePeriod[value & 0x0F];
      break;
    case 0x400F:
      if (noise_.enabled) {
        noise_.length = kLengthTable[(value >> 3) & 0x1F];
      }
      noise_.envelope.start = true;
      break;
    case 0x4010:
      dmc_.irq_enabled = (value & 0x80) != 0;
      if (!dmc_.irq_enabled) {
        dmc_.irq = false;
      }
      dmc_.loop = (value & 0x40) != 0;
      dmc_.rate_index = static_cast<u8>(value & 0x0F);
      dmc_.timer_period = kDmcRate[dmc_.rate_index];
      break;
    case 0x4011:
      dmc_.output_level = static_cast<u8>(value & 0x7F);
      break;
    case 0x4012:
      dmc_.sample_address = static_cast<u16>(0xC000 + (value * 64));
      break;
    case 0x4013:
      dmc_.sample_length = static_cast<u16>(value * 16 + 1);
      break;
    case 0x4015:
      pulse1_.enabled = (value & 0x01) != 0;
      if (!pulse1_.enabled) {
        pulse1_.length = 0;
      }
      pulse2_.enabled = (value & 0x02) != 0;
      if (!pulse2_.enabled) {
        pulse2_.length = 0;
      }
      triangle_.enabled = (value & 0x04) != 0;
      if (!triangle_.enabled) {
        triangle_.length = 0;
      }
      noise_.enabled = (value & 0x08) != 0;
      if (!noise_.enabled) {
        noise_.length = 0;
      }
      dmc_.enabled = (value & 0x10) != 0;
      if (!dmc_.enabled) {
        dmc_.bytes_remaining = 0;
      } else if (dmc_.bytes_remaining == 0) {
        dmc_.current_address = dmc_.sample_address;
        dmc_.bytes_remaining = dmc_.sample_length;
        dmc_.dma_pending = true;
        dmc_dma_pending_ = true;
      }
      dmc_.irq = false;
      break;
    case 0x4017:
      five_step_ = (value & 0x80) != 0;
      irq_inhibit_ = (value & 0x40) != 0;
      if (irq_inhibit_) {
        frame_irq_ = false;
      }
      // Write resets frame counter after 3 or 4 CPU cycles
      frame_reset_delay_ = (cpu_cycle_ & 1) ? 3 : 4;
      frame_cycle_ = 0;
      frame_step_ = 0;
      if (five_step_) {
        clock_quarter_frame();
        clock_half_frame();
      }
      break;
    default:
      break;
  }
}

u8 Apu::read_status() {
  u8 v = 0;
  if (pulse1_.length > 0) {
    v = static_cast<u8>(v | 0x01);
  }
  if (pulse2_.length > 0) {
    v = static_cast<u8>(v | 0x02);
  }
  if (triangle_.length > 0) {
    v = static_cast<u8>(v | 0x04);
  }
  if (noise_.length > 0) {
    v = static_cast<u8>(v | 0x08);
  }
  if (dmc_.bytes_remaining > 0) {
    v = static_cast<u8>(v | 0x10);
  }
  if (frame_irq_) {
    v = static_cast<u8>(v | 0x40);
  }
  if (dmc_.irq) {
    v = static_cast<u8>(v | 0x80);
  }
  frame_irq_ = false;
  return v;
}

u8 Apu::read_status_peek() const {
  u8 v = 0;
  if (pulse1_.length > 0) {
    v = static_cast<u8>(v | 0x01);
  }
  if (pulse2_.length > 0) {
    v = static_cast<u8>(v | 0x02);
  }
  if (triangle_.length > 0) {
    v = static_cast<u8>(v | 0x04);
  }
  if (noise_.length > 0) {
    v = static_cast<u8>(v | 0x08);
  }
  if (dmc_.bytes_remaining > 0) {
    v = static_cast<u8>(v | 0x10);
  }
  if (frame_irq_) {
    v = static_cast<u8>(v | 0x40);
  }
  if (dmc_.irq) {
    v = static_cast<u8>(v | 0x80);
  }
  return v;
}

void Apu::dmc_dma_complete(u8 sample_byte) {
  dmc_.sample_buffer = sample_byte;
  dmc_.sample_buffer_empty = false;
  dmc_dma_pending_ = false;
  dmc_.dma_pending = false;
  if (dmc_.bytes_remaining > 0) {
    dmc_.current_address = static_cast<u16>(dmc_.current_address + 1);
    if (dmc_.current_address == 0) {
      dmc_.current_address = static_cast<u16>(0x8000);
    }
    --dmc_.bytes_remaining;
    if (dmc_.bytes_remaining == 0 && dmc_.loop) {
      dmc_.current_address = dmc_.sample_address;
      dmc_.bytes_remaining = dmc_.sample_length;
    } else if (dmc_.bytes_remaining == 0 && dmc_.irq_enabled) {
      dmc_.irq = true;
    }
  }
}

void Apu::clock_quarter_frame() {
  pulse1_.clock_quarter();
  pulse2_.clock_quarter();
  triangle_.clock_quarter();
  noise_.clock_quarter();
}

void Apu::clock_half_frame() {
  pulse1_.clock_length();
  pulse1_.sweep.clock(pulse1_.timer_period);
  pulse2_.clock_length();
  pulse2_.sweep.clock(pulse2_.timer_period);
  triangle_.clock_length();
  noise_.clock_length();
}

void Apu::clock_frame_counter() {
  if (frame_reset_delay_ > 0) {
    --frame_reset_delay_;
    return;
  }
  ++frame_cycle_;
  // NTSC CPU cycles. Sequences from nesdev.
  if (!five_step_) {
    // 4-step: 3728.5, 7456.5, 11185.5, 14914.5 (approx integers)
    switch (frame_cycle_) {
      case 7457:
        clock_quarter_frame();
        break;
      case 14913:
        clock_quarter_frame();
        clock_half_frame();
        break;
      case 22371:
        clock_quarter_frame();
        break;
      case 29829:
        clock_quarter_frame();
        clock_half_frame();
        if (!irq_inhibit_) {
          frame_irq_ = true;
        }
        frame_cycle_ = 0;
        break;
      default:
        break;
    }
  } else {
    switch (frame_cycle_) {
      case 7457:
        clock_quarter_frame();
        break;
      case 14913:
        clock_quarter_frame();
        clock_half_frame();
        break;
      case 22371:
        clock_quarter_frame();
        break;
      case 29829:
        frame_cycle_ = 0;
        break;
      case 37281:
        clock_quarter_frame();
        clock_half_frame();
        frame_cycle_ = 0;
        break;
      default:
        break;
    }
  }
}

void Apu::mix_and_push_sample() {
  const float p_out =
      (pulse1_.output() + pulse2_.output() == 0)
          ? 0.0f
          : 95.88f / ((8128.0f / (pulse1_.output() + pulse2_.output())) + 100.0f);
  const int tnd_sum = triangle_.output() * 3 + noise_.output() * 2 + dmc_.output_level;
  const float tnd_out = (tnd_sum == 0) ? 0.0f : 159.79f / ((1.0f / (tnd_sum / 22638.0f)) + 100.0f);
  last_mixed_ = p_out + tnd_out;
  // Ring buffer (avoid deque alloc traffic in the hot path).
  if (sample_count_ == kSampleCap) {
    sample_head_ = (sample_head_ + 1) % kSampleCap;
    --sample_count_;
  }
  const std::size_t tail = (sample_head_ + sample_count_) % kSampleCap;
  sample_ring_[tail] = last_mixed_;
  ++sample_count_;
}

bool Apu::pop_sample(float& out) {
  if (sample_count_ == 0) {
    return false;
  }
  out = sample_ring_[sample_head_];
  sample_head_ = (sample_head_ + 1) % kSampleCap;
  --sample_count_;
  return true;
}

std::size_t Apu::pop_samples(float* out, std::size_t max_count) {
  std::size_t n = 0;
  while (n < max_count && sample_count_ > 0) {
    out[n++] = sample_ring_[sample_head_];
    sample_head_ = (sample_head_ + 1) % kSampleCap;
    --sample_count_;
  }
  return n;
}

std::size_t Apu::samples_available() const {
  return sample_count_;
}

void Apu::clock_cpu() {
  ++cpu_cycle_;
  clock_frame_counter();

  // Timers clock every CPU cycle for triangle; pulse/noise every other
  triangle_.clock_timer();
  if ((cpu_cycle_ & 1) == 0) {
    pulse1_.clock_timer();
    pulse2_.clock_timer();
    noise_.clock_timer();
    dmc_.clock_timer();
    if (dmc_.dma_pending) {
      dmc_dma_pending_ = true;
    }
  }

  sample_acc_ += 1.0;
  if (sample_acc_ >= cycles_per_sample_) {
    sample_acc_ -= cycles_per_sample_;
    mix_and_push_sample();
  }
}

void Apu::save_state(StateWriter& w) const {
  auto save_pulse = [&](const Pulse& p) {
    w.write_bool(p.enabled);
    w.write_u8(p.duty);
    w.write_u8(p.duty_pos);
    w.write_bool(p.length_halt);
    w.write_u8(p.length);
    w.write_u16(p.timer);
    w.write_u16(p.timer_period);
    w.write_bool(p.envelope.start);
    w.write_bool(p.envelope.loop);
    w.write_bool(p.envelope.constant);
    w.write_u8(p.envelope.volume);
    w.write_u8(p.envelope.divider);
    w.write_u8(p.envelope.decay);
    w.write_bool(p.sweep.enabled);
    w.write_bool(p.sweep.negate);
    w.write_bool(p.sweep.reload);
    w.write_u8(p.sweep.period);
    w.write_u8(p.sweep.shift);
    w.write_u8(p.sweep.divider);
  };
  save_pulse(pulse1_);
  save_pulse(pulse2_);

  w.write_bool(triangle_.enabled);
  w.write_bool(triangle_.linear_reload);
  w.write_u8(triangle_.linear_reload_value);
  w.write_u8(triangle_.linear_counter);
  w.write_bool(triangle_.control);
  w.write_u8(triangle_.length);
  w.write_u16(triangle_.timer);
  w.write_u16(triangle_.timer_period);
  w.write_u8(triangle_.seq_pos);

  w.write_bool(noise_.enabled);
  w.write_bool(noise_.mode);
  w.write_bool(noise_.length_halt);
  w.write_u8(noise_.length);
  w.write_u16(noise_.shift);
  w.write_u16(noise_.timer);
  w.write_u16(noise_.timer_period);
  w.write_bool(noise_.envelope.start);
  w.write_bool(noise_.envelope.loop);
  w.write_bool(noise_.envelope.constant);
  w.write_u8(noise_.envelope.volume);
  w.write_u8(noise_.envelope.divider);
  w.write_u8(noise_.envelope.decay);

  w.write_bool(dmc_.enabled);
  w.write_bool(dmc_.irq_enabled);
  w.write_bool(dmc_.loop);
  w.write_bool(dmc_.irq);
  w.write_u8(dmc_.rate_index);
  w.write_u8(dmc_.output_level);
  w.write_u8(dmc_.bits_remaining);
  w.write_u8(dmc_.sample_buffer);
  w.write_bool(dmc_.sample_buffer_empty);
  w.write_u8(dmc_.shift);
  w.write_u16(dmc_.sample_address);
  w.write_u16(dmc_.sample_length);
  w.write_u16(dmc_.current_address);
  w.write_u16(dmc_.bytes_remaining);
  w.write_u16(dmc_.timer);
  w.write_u16(dmc_.timer_period);

  w.write_bool(five_step_);
  w.write_bool(irq_inhibit_);
  w.write_bool(frame_irq_);
  w.write_bool(dmc_irq_);
  w.write_u32(frame_cycle_);
  w.write_u32(frame_reset_delay_);
  w.write_u8(frame_step_);
  w.write_u32(cpu_cycle_);
}

void Apu::load_state(StateReader& r) {
  auto load_pulse = [&](Pulse& p) {
    p.enabled = r.read_bool();
    p.duty = r.read_u8();
    p.duty_pos = r.read_u8();
    p.length_halt = r.read_bool();
    p.length = r.read_u8();
    p.timer = r.read_u16();
    p.timer_period = r.read_u16();
    p.envelope.start = r.read_bool();
    p.envelope.loop = r.read_bool();
    p.envelope.constant = r.read_bool();
    p.envelope.volume = r.read_u8();
    p.envelope.divider = r.read_u8();
    p.envelope.decay = r.read_u8();
    p.sweep.enabled = r.read_bool();
    p.sweep.negate = r.read_bool();
    p.sweep.reload = r.read_bool();
    p.sweep.period = r.read_u8();
    p.sweep.shift = r.read_u8();
    p.sweep.divider = r.read_u8();
    p.sweep.pulse1 = (&p == &pulse1_);
  };
  load_pulse(pulse1_);
  load_pulse(pulse2_);
  pulse1_.sweep.pulse1 = true;
  pulse2_.sweep.pulse1 = false;

  triangle_.enabled = r.read_bool();
  triangle_.linear_reload = r.read_bool();
  triangle_.linear_reload_value = r.read_u8();
  triangle_.linear_counter = r.read_u8();
  triangle_.control = r.read_bool();
  triangle_.length = r.read_u8();
  triangle_.timer = r.read_u16();
  triangle_.timer_period = r.read_u16();
  triangle_.seq_pos = r.read_u8();

  noise_.enabled = r.read_bool();
  noise_.mode = r.read_bool();
  noise_.length_halt = r.read_bool();
  noise_.length = r.read_u8();
  noise_.shift = r.read_u16();
  noise_.timer = r.read_u16();
  noise_.timer_period = r.read_u16();
  noise_.envelope.start = r.read_bool();
  noise_.envelope.loop = r.read_bool();
  noise_.envelope.constant = r.read_bool();
  noise_.envelope.volume = r.read_u8();
  noise_.envelope.divider = r.read_u8();
  noise_.envelope.decay = r.read_u8();

  dmc_.enabled = r.read_bool();
  dmc_.irq_enabled = r.read_bool();
  dmc_.loop = r.read_bool();
  dmc_.irq = r.read_bool();
  dmc_.rate_index = r.read_u8();
  dmc_.output_level = r.read_u8();
  dmc_.bits_remaining = r.read_u8();
  dmc_.sample_buffer = r.read_u8();
  dmc_.sample_buffer_empty = r.read_bool();
  dmc_.shift = r.read_u8();
  dmc_.sample_address = r.read_u16();
  dmc_.sample_length = r.read_u16();
  dmc_.current_address = r.read_u16();
  dmc_.bytes_remaining = r.read_u16();
  dmc_.timer = r.read_u16();
  dmc_.timer_period = r.read_u16();

  five_step_ = r.read_bool();
  irq_inhibit_ = r.read_bool();
  frame_irq_ = r.read_bool();
  dmc_irq_ = r.read_bool();
  frame_cycle_ = r.read_u32();
  frame_reset_delay_ = r.read_u32();
  frame_step_ = r.read_u8();
  cpu_cycle_ = r.read_u32();
}

}  // namespace nesemu
