#pragma once

#include "state.hpp"
#include "types.hpp"

#include <array>

namespace nesemu {

class Mapper;

class Ppu {
public:
  void connect(Mapper* mapper) { mapper_ = mapper; }

  void reset();
  void tick();  // one PPU dot

  // CPU-visible registers $2000-$2007
  u8 reg_read(u16 addr);
  void reg_write(u16 addr, u8 value);
  u8 reg_peek(u16 addr) const;

  bool nmi_line() const { return nmi_output_; }
  bool frame_ready() const { return frame_ready_; }
  void clear_frame_ready() { frame_ready_ = false; }

  u64 frame() const { return frame_; }
  u16 scanline() const { return scanline_; }
  u16 dot() const { return dot_; }

  const Framebuffer& framebuffer() const { return fb_; }

  void oam_dma_write(u8 value);

  void save_state(StateWriter& w) const;
  void load_state(StateReader& r);

  // Debug
  u8 vram_peek(u16 addr) const;
  u8 palette_peek(u8 idx) const;
  const std::array<u8, 256>& oam() const { return oam_; }

private:
  Mapper* mapper_ = nullptr;

  // Registers
  u8 control_ = 0;   // $2000
  u8 mask_ = 0;      // $2001
  u8 status_ = 0;    // $2002
  u8 oam_addr_ = 0;  // $2003
  u8 data_buffer_ = 0;

  // Scroll / VRAM address
  u16 v_ = 0;
  u16 t_ = 0;
  u8 x_ = 0;
  bool w_ = false;

  // Timing
  u16 scanline_ = 0;
  u16 dot_ = 0;
  u64 frame_ = 0;
  bool odd_frame_ = false;
  bool frame_ready_ = false;
  bool nmi_output_ = false;
  bool prev_nmi_ = false;

  // Memories
  std::array<u8, 0x800> nametable_ram_{};
  std::array<u8, 32> palette_ram_{};
  std::array<u8, 256> oam_{};
  std::array<u8, 32> secondary_oam_{};

  // Background pipeline
  u8 nt_byte_ = 0;
  u8 at_byte_ = 0;
  u8 bg_lo_ = 0;
  u8 bg_hi_ = 0;
  u16 bg_lo_sh_ = 0;
  u16 bg_hi_sh_ = 0;
  u16 at_lo_sh_ = 0;
  u16 at_hi_sh_ = 0;
  // Scroll snapshot for the scanline currently being painted.
  u16 line_x_ = 0;  // nametable pixel X for screen x=0
  u16 line_y_ = 0;  // nametable pixel Y
  mutable u16 tile_x_ = 0xFFFF;
  mutable u8 tile_lo_ = 0;
  mutable u8 tile_hi_ = 0;
  mutable u8 tile_at_ = 0;
  mutable u8 tile_fine_y_ = 0xFF;

  // Sprite pipeline (per-scanline)
  struct SpriteOut {
    u8 x = 0;
    u8 pattern_lo = 0;
    u8 pattern_hi = 0;
    u8 attrs = 0;
    bool sprite0 = false;
  };
  std::array<SpriteOut, 8> sprites_{};
  u8 sprite_count_ = 0;
  bool sprite_zero_on_scanline_ = false;
  u8 sprite_zero_hit_x_ = 0;
  bool sprite_zero_hit_possible_ = false;

  Framebuffer fb_{};

  u8 ntram_read(u16 addr) const;
  void ntram_write(u16 addr, u8 value);
  u8 palette_read(u16 addr) const;
  void palette_write(u16 addr, u8 value);

  u8 read_vram(u16 addr);
  void write_vram(u16 addr, u8 value);

  void increment_x();
  void increment_y();
  void copy_horizontal();
  void copy_vertical();
  void bg_pixel_at(int x, u8& pix, u8& pal) const;
  mutable u16 cache_key_ = 0xFFFF;
  void snapshot_scroll();
  void load_background_shifters();
  void shift_background();
  void fetch_bg_tile();
  void evaluate_sprites(int target_y);
  void render_pixel();
  u8 pixel_color(u8 bg_pixel, u8 bg_pal, u8 sp_pixel, u8 sp_pal, bool sp_priority, bool sp_is_zero) const;

  bool rendering_enabled() const { return (mask_ & 0x18) != 0; }
  bool show_bg() const { return (mask_ & 0x08) != 0; }
  bool show_sp() const { return (mask_ & 0x10) != 0; }
  int sprite_height() const { return (control_ & 0x20) ? 16 : 8; }
  u16 bg_pattern_base() const { return (control_ & 0x10) ? 0x1000 : 0x0000; }
  u16 sp_pattern_base() const { return (control_ & 0x08) ? 0x1000 : 0x0000; }
};

}  // namespace nesemu
