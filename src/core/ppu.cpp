#include "nesemu/ppu.hpp"

#include "nesemu/mapper.hpp"

#include <cstring>

namespace nesemu {

void Ppu::reset() {
  control_ = 0;
  mask_ = 0;
  status_ = 0;
  oam_addr_ = 0;
  data_buffer_ = 0;
  v_ = t_ = 0;
  x_ = 0;
  w_ = false;
  scanline_ = 261;  // pre-render
  dot_ = 0;
  frame_ = 0;
  odd_frame_ = false;
  frame_ready_ = false;
  nmi_output_ = false;
  prev_nmi_ = false;
  nametable_ram_.fill(0);
  palette_ram_.fill(0x0F);
  oam_.fill(0);
  secondary_oam_.fill(0xFF);
  nt_byte_ = at_byte_ = bg_lo_ = bg_hi_ = 0;
  bg_lo_sh_ = bg_hi_sh_ = 0;
  at_lo_sh_ = at_hi_sh_ = 0;
  sprite_count_ = 0;
  sprite_zero_on_scanline_ = false;
  sprite_zero_hit_possible_ = false;
  fb_.clear();
}

u8 Ppu::ntram_read(u16 addr) const {
  addr = static_cast<u16>(addr & 0x0FFF);
  // Nametable mirroring via mapper
  Mirroring m = Mirroring::Horizontal;
  if (mapper_) {
    m = mapper_->mirroring();
  }
  u16 index = 0;
  switch (m) {
    case Mirroring::Horizontal:
      // $2000=$2400, $2800=$2C00
      index = static_cast<u16>(((addr >> 1) & 0x400) | (addr & 0x3FF));
      break;
    case Mirroring::Vertical:
      // $2000=$2800, $2400=$2C00
      index = static_cast<u16>(addr & 0x7FF);
      break;
    case Mirroring::SingleScreenA:
      index = static_cast<u16>(addr & 0x3FF);
      break;
    case Mirroring::SingleScreenB:
      index = static_cast<u16>(0x400 | (addr & 0x3FF));
      break;
    case Mirroring::FourScreen:
      index = static_cast<u16>(addr & 0x7FF);
      break;
  }
  return nametable_ram_[index & 0x7FF];
}

void Ppu::ntram_write(u16 addr, u8 value) {
  addr = static_cast<u16>(addr & 0x0FFF);
  Mirroring m = Mirroring::Horizontal;
  if (mapper_) {
    m = mapper_->mirroring();
  }
  u16 index = 0;
  switch (m) {
    case Mirroring::Horizontal:
      index = static_cast<u16>(((addr >> 1) & 0x400) | (addr & 0x3FF));
      break;
    case Mirroring::Vertical:
      index = static_cast<u16>(addr & 0x7FF);
      break;
    case Mirroring::SingleScreenA:
      index = static_cast<u16>(addr & 0x3FF);
      break;
    case Mirroring::SingleScreenB:
      index = static_cast<u16>(0x400 | (addr & 0x3FF));
      break;
    case Mirroring::FourScreen:
      index = static_cast<u16>(addr & 0x7FF);
      break;
  }
  nametable_ram_[index & 0x7FF] = value;
}

u8 Ppu::palette_read(u16 addr) const {
  addr = static_cast<u16>(addr & 0x1F);
  if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) {
    addr = static_cast<u16>(addr & ~0x10);
  }
  return palette_ram_[addr];
}

void Ppu::palette_write(u16 addr, u8 value) {
  addr = static_cast<u16>(addr & 0x1F);
  if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) {
    addr = static_cast<u16>(addr & ~0x10);
  }
  palette_ram_[addr] = static_cast<u8>(value & 0x3F);
}

u8 Ppu::read_vram(u16 addr) {
  addr = static_cast<u16>(addr & 0x3FFF);
  if (mapper_) {
    mapper_->observe_ppu_address(addr);
  }
  if (addr < 0x2000) {
    return mapper_ ? mapper_->ppu_read(addr) : 0;
  }
  if (addr < 0x3F00) {
    return ntram_read(addr);
  }
  return palette_read(addr);
}

void Ppu::write_vram(u16 addr, u8 value) {
  addr = static_cast<u16>(addr & 0x3FFF);
  if (mapper_) {
    mapper_->observe_ppu_address(addr);
  }
  if (addr < 0x2000) {
    if (mapper_) {
      mapper_->ppu_write(addr, value);
    }
    return;
  }
  if (addr < 0x3F00) {
    ntram_write(addr, value);
    return;
  }
  palette_write(addr, value);
}

u8 Ppu::reg_read(u16 addr) {
  switch (addr & 0x07) {
    case 2: {
      const u8 result = static_cast<u8>((status_ & 0xE0) | (data_buffer_ & 0x1F));
      status_ = static_cast<u8>(status_ & ~0x80);  // clear vblank
      w_ = false;
      // Reading $2002 clears vblank; NMI line follows vblank&enable.
      nmi_output_ = (status_ & 0x80) && (control_ & 0x80);
      return result;
    }
    case 4:
      return oam_[oam_addr_];
    case 7: {
      const u16 a = static_cast<u16>(v_ & 0x3FFF);
      u8 value = data_buffer_;
      data_buffer_ = read_vram(a);
      if (a >= 0x3F00) {
        value = palette_read(a);
        data_buffer_ = ntram_read(a);
      }
      v_ = static_cast<u16>(v_ + ((control_ & 0x04) ? 32 : 1));
      if (mapper_) {
        mapper_->observe_ppu_address(static_cast<u16>(v_ & 0x3FFF));
      }
      return value;
    }
    default:
      return data_buffer_;
  }
}

void Ppu::reg_write(u16 addr, u8 value) {
  data_buffer_ = value;
  switch (addr & 0x07) {
    case 0: {
      control_ = value;
      t_ = static_cast<u16>((t_ & 0xF3FF) | ((value & 0x03) << 10));
      // NMI line = vblank_flag && nmi_enable (edge-triggered externally).
      nmi_output_ = ((status_ & 0x80) != 0) && ((control_ & 0x80) != 0);
      break;
    }
    case 1:
      mask_ = value;
      break;
    case 3:
      oam_addr_ = value;
      break;
    case 4:
      oam_[oam_addr_++] = value;
      break;
    case 5:
      if (!w_) {
        x_ = static_cast<u8>(value & 0x07);
        t_ = static_cast<u16>((t_ & 0xFFE0) | (value >> 3));
        w_ = true;
      } else {
        t_ = static_cast<u16>((t_ & 0x8C1F) | ((value & 0x07) << 12) | ((value & 0xF8) << 2));
        w_ = false;
      }
      break;
    case 6:
      if (!w_) {
        t_ = static_cast<u16>((t_ & 0x00FF) | ((value & 0x3F) << 8));
        w_ = true;
        // A12 can change on the high-byte write alone (MMC3 IRQ clocking).
        if (mapper_) {
          mapper_->observe_ppu_address(static_cast<u16>(t_ & 0x3FFF));
        }
      } else {
        t_ = static_cast<u16>((t_ & 0xFF00) | value);
        v_ = t_;
        if (mapper_) {
          mapper_->observe_ppu_address(static_cast<u16>(v_ & 0x3FFF));
        }
        w_ = false;
      }
      break;
    case 7: {
      write_vram(v_, value);
      v_ = static_cast<u16>(v_ + ((control_ & 0x04) ? 32 : 1));
      if (mapper_) {
        mapper_->observe_ppu_address(static_cast<u16>(v_ & 0x3FFF));
      }
      break;
    }
    default:
      break;
  }
}

u8 Ppu::reg_peek(u16 addr) const {
  switch (addr & 0x07) {
    case 0:
      return control_;
    case 1:
      return mask_;
    case 2:
      return status_;
    case 3:
      return oam_addr_;
    case 4:
      return oam_[oam_addr_];
    case 7: {
      const u16 a = static_cast<u16>(v_ & 0x3FFF);
      if (a >= 0x3F00) {
        return palette_read(a);
      }
      return data_buffer_;
    }
    default:
      return data_buffer_;
  }
}

void Ppu::oam_dma_write(u8 value) {
  oam_[oam_addr_++] = value;
}

void Ppu::increment_x() {
  if ((v_ & 0x001F) == 31) {
    v_ = static_cast<u16>(v_ & ~0x001F);
    v_ ^= 0x0400;
  } else {
    v_ = static_cast<u16>(v_ + 1);
  }
}

void Ppu::increment_y() {
  if ((v_ & 0x7000) != 0x7000) {
    v_ = static_cast<u16>(v_ + 0x1000);
  } else {
    v_ = static_cast<u16>(v_ & ~0x7000);
    u16 y = static_cast<u16>((v_ & 0x03E0) >> 5);
    if (y == 29) {
      y = 0;
      v_ ^= 0x0800;
    } else if (y == 31) {
      y = 0;
    } else {
      y++;
    }
    v_ = static_cast<u16>((v_ & ~0x03E0) | (y << 5));
  }
}

void Ppu::copy_horizontal() {
  v_ = static_cast<u16>((v_ & ~0x041F) | (t_ & 0x041F));
}

void Ppu::copy_vertical() {
  v_ = static_cast<u16>((v_ & ~0x7BE0) | (t_ & 0x7BE0));
}

void Ppu::load_background_shifters() {}

void Ppu::shift_background() {}

void Ppu::fetch_bg_tile() {
  // Unused — BG pixels are resolved directly from the scroll snapshot.
}

// Resolve BG pixel for screen x on the current line (scroll snapshotted at line start).
void Ppu::bg_pixel_at(int x, u8& pix, u8& pal) const {
  pix = 0;
  pal = 0;
  const u16 sx = static_cast<u16>(line_x_ + x);
  const u16 sy = line_y_;
  const u16 nt_x = static_cast<u16>((sx >> 8) & 1);
  const u16 nt_y = static_cast<u16>((sy >> 8) & 1);
  const u16 coarse_x = static_cast<u16>((sx >> 3) & 0x1F);
  const u16 coarse_y = static_cast<u16>((sy >> 3) & 0x1F);
  const u16 fine_x = static_cast<u16>(sx & 7);
  const u16 fine_y = static_cast<u16>(sy & 7);
  const u16 key = static_cast<u16>(sx >> 3);  // tile column in 512px space
  // Cache the current tile's pattern/attr (const method mutates mutable cache).
  if (key != cache_key_ || fine_y != tile_fine_y_) {
    cache_key_ = key;
    tile_fine_y_ = static_cast<u8>(fine_y);
    const u16 nt = static_cast<u16>(0x2000 | (nt_y << 11) | (nt_x << 10) | (coarse_y << 5) | coarse_x);
    const u8 tile = ntram_read(nt);
    const u16 addr = static_cast<u16>(bg_pattern_base() + tile * 16 + fine_y);
    tile_lo_ = mapper_ ? mapper_->ppu_peek(addr) : 0;
    tile_hi_ = mapper_ ? mapper_->ppu_peek(static_cast<u16>(addr + 8)) : 0;
    const u16 at_addr = static_cast<u16>(0x23C0 | (nt_y << 11) | (nt_x << 10) |
                                        ((coarse_y >> 2) << 3) | (coarse_x >> 2));
    u8 at = ntram_read(at_addr);
    if (coarse_y & 2) {
      at >>= 4;
    }
    if (coarse_x & 2) {
      at >>= 2;
    }
    tile_at_ = static_cast<u8>(at & 3);
  }
  const int bit = 7 - static_cast<int>(fine_x);
  pix = static_cast<u8>(((tile_lo_ >> bit) & 1) | (((tile_hi_ >> bit) & 1) << 1));
  pal = tile_at_;
}

void Ppu::snapshot_scroll() {
  // X comes from t_ + fine X: v_ has been advanced by the end-of-line prefetch.
  // Y comes from v_ (already incremented per scanline via increment_y).
  line_x_ = static_cast<u16>(((t_ & 0x1F) << 3) + x_ + (((t_ >> 10) & 1) << 8));
  line_y_ = static_cast<u16>((((v_ >> 5) & 0x1F) << 3) + ((v_ >> 12) & 7) +
                             (((v_ >> 11) & 1) << 8));
}

void Ppu::evaluate_sprites(int target_y) {
  sprite_count_ = 0;
  sprite_zero_on_scanline_ = false;
  sprite_zero_hit_possible_ = false;
  sprites_ = {};

  const int y = target_y;
  const int h = sprite_height();
  int n = 0;
  for (int i = 0; i < 64; ++i) {
    const int sy = static_cast<int>(oam_[static_cast<std::size_t>(i * 4 + 0)]);
    // OAM Y is the first visible scanline of the sprite (row 0).
    const int row = y - sy;
    if (row < 0 || row >= h) {
      continue;
    }
    if (n < 8) {
      SpriteOut& s = sprites_[static_cast<std::size_t>(n)];
      s.sprite0 = (i == 0);
      if (i == 0) {
        sprite_zero_on_scanline_ = true;
        sprite_zero_hit_possible_ = true;
      }
      const u8 tile = oam_[static_cast<std::size_t>(i * 4 + 1)];
      const u8 attr = oam_[static_cast<std::size_t>(i * 4 + 2)];
      const u8 sx = oam_[static_cast<std::size_t>(i * 4 + 3)];
      s.x = sx;
      s.attrs = attr;

      u16 addr = 0;
      if (h == 8) {
        u16 r = static_cast<u16>(row);
        if (attr & 0x80) {
          r = static_cast<u16>(7 - r);
        }
        addr = static_cast<u16>(sp_pattern_base() + tile * 16 + r);
      } else {
        u16 r = static_cast<u16>(row);
        if (attr & 0x80) {
          r = static_cast<u16>(15 - r);
        }
        u16 table = static_cast<u16>((tile & 0x01) * 0x1000);
        u8 t = static_cast<u8>(tile & 0xFE);
        if (r >= 8) {
          t = static_cast<u8>(t + 1);
          r = static_cast<u16>(r - 8);
        }
        addr = static_cast<u16>(table + t * 16 + r);
      }
      s.pattern_lo = read_vram(addr);
      s.pattern_hi = read_vram(static_cast<u16>(addr + 8));
      if (attr & 0x40) {
        // horizontal flip: reverse bits
        auto rev = [](u8 b) {
          b = static_cast<u8>((b & 0xF0) >> 4 | (b & 0x0F) << 4);
          b = static_cast<u8>((b & 0xCC) >> 2 | (b & 0x33) << 2);
          b = static_cast<u8>((b & 0xAA) >> 1 | (b & 0x55) << 1);
          return b;
        };
        s.pattern_lo = rev(s.pattern_lo);
        s.pattern_hi = rev(s.pattern_hi);
      }
      ++n;
    }
  }
  if (n > 8) {
    n = 8;
    status_ = static_cast<u8>(status_ | 0x20);  // overflow (approximate)
  }
  sprite_count_ = static_cast<u8>(n);
}

u8 Ppu::pixel_color(u8 bg_pixel, u8 bg_pal, u8 sp_pixel, u8 sp_pal, bool sp_priority,
                    bool sp_is_zero) const {
  (void)sp_is_zero;
  if (bg_pixel == 0 && sp_pixel == 0) {
    return palette_read(0);
  }
  if (bg_pixel == 0) {
    return palette_read(static_cast<u16>(0x10 + sp_pal * 4 + sp_pixel));
  }
  if (sp_pixel == 0) {
    return palette_read(static_cast<u16>(bg_pal * 4 + bg_pixel));
  }
  // both opaque: sprite zero hit
  return sp_priority ? palette_read(static_cast<u16>(0x10 + sp_pal * 4 + sp_pixel))
                     : palette_read(static_cast<u16>(bg_pal * 4 + bg_pixel));
}

void Ppu::render_pixel() {
  const int x = static_cast<int>(dot_ - 1);
  const int y = static_cast<int>(scanline_);
  if (x < 0 || x >= kScreenWidth || y < 0 || y >= kScreenHeight) {
    return;
  }

  u8 bg_pixel = 0;
  u8 bg_pal = 0;
  if (show_bg() && (x >= 8 || (mask_ & 0x02))) {
    bg_pixel_at(x, bg_pixel, bg_pal);
  }

  u8 sp_pixel = 0;
  u8 sp_pal = 0;
  bool sp_priority = false;
  bool sp_is_zero = false;
  if (show_sp() && (x >= 8 || (mask_ & 0x04))) {
    for (int i = 0; i < sprite_count_; ++i) {
      const SpriteOut& s = sprites_[static_cast<std::size_t>(i)];
      const int sx = x - s.x;
      if (sx < 0 || sx >= 8) {
        continue;
      }
      const int bit = 7 - sx;
      const u8 p = static_cast<u8>(((s.pattern_lo >> bit) & 1) | (((s.pattern_hi >> bit) & 1) << 1));
      if (p == 0) {
        continue;
      }
      sp_pixel = p;
      sp_pal = static_cast<u8>(s.attrs & 0x03);
      sp_priority = (s.attrs & 0x20) == 0;
      sp_is_zero = s.sprite0;
      break;
    }
  }

  if (sprite_zero_hit_possible_ && sp_is_zero && bg_pixel && sp_pixel && x != 255) {
    if (show_bg() && show_sp()) {
      status_ = static_cast<u8>(status_ | 0x40);
    }
  }

  u8 color = pixel_color(bg_pixel, bg_pal, sp_pixel, sp_pal, sp_priority, sp_is_zero);
  if (mask_ & 0x01) {
    color = static_cast<u8>(color & 0x30);
  }
  fb_.set(x, y, color, static_cast<u8>((mask_ >> 5) & 0x07));
}

void Ppu::tick() {
  // A12-low counter for MMC3 only; other mappers ignore this.
  if (mapper_) {
    mapper_->clock_ppu();
  }

  const bool visible_line = scanline_ < 240;
  const bool pre_render = scanline_ == 261;
  const bool render_line = visible_line || pre_render;

  if (render_line && rendering_enabled()) {
    // Keep v_ incrementing for MMC3 A12 / mid-frame scroll; BG pixels use snapshot.
    const bool fetch_cycle =
        (dot_ >= 1 && dot_ <= 256) || (dot_ >= 321 && dot_ <= 336);
    if (fetch_cycle) {
      switch ((dot_ - 1) % 8) {
        case 0:
          nt_byte_ = read_vram(static_cast<u16>(0x2000 | (v_ & 0x0FFF)));
          break;
        case 2: {
          const u16 addr = static_cast<u16>(0x23C0 | (v_ & 0x0C00) | ((v_ >> 4) & 0x38) |
                                           ((v_ >> 2) & 0x07));
          u8 at = read_vram(addr);
          at_byte_ = static_cast<u8>(at & 0x03);
          break;
        }
        case 4: {
          const u16 fine_y = static_cast<u16>((v_ >> 12) & 0x07);
          bg_lo_ = read_vram(static_cast<u16>(bg_pattern_base() + (nt_byte_ * 16) + fine_y));
          break;
        }
        case 6: {
          const u16 fine_y = static_cast<u16>((v_ >> 12) & 0x07);
          bg_hi_ =
              read_vram(static_cast<u16>(bg_pattern_base() + (nt_byte_ * 16) + fine_y + 8));
          break;
        }
        case 7:
          increment_x();
          break;
        default:
          break;
      }
    }

    if (visible_line && dot_ == 1) {
      snapshot_scroll();
    }
    if (visible_line && dot_ >= 1 && dot_ <= 256) {
      render_pixel();
    }

    if (dot_ == 256) {
      increment_y();
    }
    if (dot_ == 257) {
      copy_horizontal();
      if (rendering_enabled()) {
        const int next_y = (scanline_ == 261) ? 0 : (scanline_ + 1);
        if (next_y < 240) {
          evaluate_sprites(next_y);
        } else {
          sprite_count_ = 0;
          sprite_zero_hit_possible_ = false;
        }
      }
    }
    if (pre_render && dot_ >= 280 && dot_ <= 304) {
      copy_vertical();
    }
  } else if (visible_line && dot_ >= 1 && dot_ <= 256) {
    u8 color = palette_read(0);
    if (mask_ & 0x01) {
      color = static_cast<u8>(color & 0x30);
    }
    fb_.set(static_cast<int>(dot_ - 1), static_cast<int>(scanline_), color,
            static_cast<u8>((mask_ >> 5) & 0x07));
  }

  if (scanline_ == 241 && dot_ == 1) {
    status_ = static_cast<u8>(status_ | 0x80);
    frame_ready_ = true;
    nmi_output_ = ((status_ & 0x80) != 0) && ((control_ & 0x80) != 0);
  }
  if (scanline_ == 261 && dot_ == 1) {
    status_ = static_cast<u8>(status_ & ~0xE0);
    nmi_output_ = false;
  }

  ++dot_;
  bool skip = false;
  if (scanline_ == 261 && dot_ == 340 && odd_frame_ && rendering_enabled()) {
    skip = true;
  }
  if (dot_ > 340 || skip) {
    dot_ = 0;
    ++scanline_;
    if (scanline_ > 261) {
      scanline_ = 0;
      ++frame_;
      odd_frame_ = !odd_frame_;
    }
  }
}

void Ppu::save_state(StateWriter& w) const {
  w.write_u8(control_);
  w.write_u8(mask_);
  w.write_u8(status_);
  w.write_u8(oam_addr_);
  w.write_u8(data_buffer_);
  w.write_u16(v_);
  w.write_u16(t_);
  w.write_u8(x_);
  w.write_bool(w_);
  w.write_u16(scanline_);
  w.write_u16(dot_);
  w.write_u64(frame_);
  w.write_bool(odd_frame_);
  w.write_bool(nmi_output_);
  w.write_bytes(nametable_ram_.data(), nametable_ram_.size());
  w.write_bytes(palette_ram_.data(), palette_ram_.size());
  w.write_bytes(oam_.data(), oam_.size());
  w.write_u8(bg_lo_);
  w.write_u8(bg_hi_);
  w.write_u8(at_byte_);
  w.write_u8(nt_byte_);
  w.write_u16(bg_lo_sh_);
  w.write_u16(bg_hi_sh_);
  w.write_u16(at_lo_sh_);
  w.write_u16(at_hi_sh_);
  w.write_u8(sprite_count_);
  w.write_bool(sprite_zero_on_scanline_);
  w.write_bool(sprite_zero_hit_possible_);
  for (const auto& s : sprites_) {
    w.write_u8(s.x);
    w.write_u8(s.pattern_lo);
    w.write_u8(s.pattern_hi);
    w.write_u8(s.attrs);
    w.write_bool(s.sprite0);
  }
}

void Ppu::load_state(StateReader& r) {
  control_ = r.read_u8();
  mask_ = r.read_u8();
  status_ = r.read_u8();
  oam_addr_ = r.read_u8();
  data_buffer_ = r.read_u8();
  v_ = r.read_u16();
  t_ = r.read_u16();
  x_ = r.read_u8();
  w_ = r.read_bool();
  scanline_ = r.read_u16();
  dot_ = r.read_u16();
  frame_ = r.read_u64();
  odd_frame_ = r.read_bool();
  nmi_output_ = r.read_bool();
  r.read_bytes(nametable_ram_.data(), nametable_ram_.size());
  r.read_bytes(palette_ram_.data(), palette_ram_.size());
  r.read_bytes(oam_.data(), oam_.size());
  bg_lo_ = r.read_u8();
  bg_hi_ = r.read_u8();
  at_byte_ = r.read_u8();
  nt_byte_ = r.read_u8();
  bg_lo_sh_ = r.read_u16();
  bg_hi_sh_ = r.read_u16();
  at_lo_sh_ = r.read_u16();
  at_hi_sh_ = r.read_u16();
  sprite_count_ = r.read_u8();
  sprite_zero_on_scanline_ = r.read_bool();
  sprite_zero_hit_possible_ = r.read_bool();
  for (auto& s : sprites_) {
    s.x = r.read_u8();
    s.pattern_lo = r.read_u8();
    s.pattern_hi = r.read_u8();
    s.attrs = r.read_u8();
    s.sprite0 = r.read_bool();
  }
}

u8 Ppu::vram_peek(u16 addr) const {
  addr = static_cast<u16>(addr & 0x3FFF);
  if (addr < 0x2000) {
    return mapper_ ? mapper_->ppu_peek(addr) : 0;
  }
  if (addr < 0x3F00) {
    return ntram_read(addr);
  }
  return palette_read(addr);
}

u8 Ppu::palette_peek(u8 idx) const {
  return palette_read(idx);
}

}  // namespace nesemu
