// Measure horizontal scroll jitter while holding Right (SMB-style).
// Usage: nesemu_sim <rom> [start_frames] [move_frames]
#include "nesemu/machine.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

using nesemu::u8;
using nesemu::u16;
using nesemu::u32;
using nesemu::u64;

namespace {

#ifdef _WIN32
std::wstring to_wide(const std::string& path) {
  int n = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
  if (n <= 0) {
    n = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, nullptr, 0);
  }
  if (n > 0) {
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') {
      w.pop_back();
    }
    return w;
  }
  return std::wstring(path.begin(), path.end());
}
std::string to_utf8(const wchar_t* ws) {
  if (!ws) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 0) return {};
  std::string s(static_cast<std::size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), n, nullptr, nullptr);
  return s;
}
nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(to_wide(path).c_str(), std::ios::binary);
  if (!f) return {};
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
#else
nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return {};
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
#endif

int left_unstable(const nesemu::Framebuffer& a, const nesemu::Framebuffer& b, int y) {
  int n = 0;
  for (int x = 0; x < 16; ++x) {
    const std::size_t i = static_cast<std::size_t>(y * nesemu::kScreenWidth + x);
    if (a.pixels[i] != b.pixels[i]) {
      ++n;
    }
  }
  return n;
}

int full_unstable(const nesemu::Framebuffer& a, const nesemu::Framebuffer& b) {
  int n = 0;
  for (int i = 0; i < nesemu::kScreenWidth * nesemu::kScreenHeight; ++i) {
    if (a.pixels[static_cast<std::size_t>(i)] != b.pixels[static_cast<std::size_t>(i)]) {
      ++n;
    }
  }
  return n;
}

// Estimate horizontal pixel shift of one row (best match in -3..3).
int hshift_row(const nesemu::Framebuffer& a, const nesemu::Framebuffer& b, int y) {
  int best = 0;
  int best_err = 1 << 30;
  for (int s = -3; s <= 3; ++s) {
    int err = 0;
    for (int x = 16; x < 240; ++x) {
      const int x2 = x + s;
      if (x2 < 0 || x2 >= nesemu::kScreenWidth) {
        ++err;
        continue;
      }
      const std::size_t ia = static_cast<std::size_t>(y * nesemu::kScreenWidth + x);
      const std::size_t ib = static_cast<std::size_t>(y * nesemu::kScreenWidth + x2);
      if (a.pixels[ia] != b.pixels[ib]) {
        ++err;
      }
    }
    if (err < best_err) {
      best_err = err;
      best = s;
    }
  }
  return best;
}

}  // namespace

int main(int argc, char** argv) {
  std::string rom_path;
  int warmup = 120;
  int move_frames = 180;
#ifdef _WIN32
  int wargc = 0;
  LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
  if (wargv) {
    if (wargc >= 2) rom_path = to_utf8(wargv[1]);
    if (wargc >= 3) warmup = static_cast<int>(wcstol(wargv[2], nullptr, 10));
    if (wargc >= 4) move_frames = static_cast<int>(wcstol(wargv[3], nullptr, 10));
    LocalFree(wargv);
  }
#endif
  if (rom_path.empty() && argc >= 2) {
    rom_path = argv[1];
  }
  if (rom_path.empty() && argc >= 3) warmup = std::atoi(argv[2]);
  if (rom_path.empty() && argc >= 4) move_frames = std::atoi(argv[3]);
  if (rom_path.empty()) {
    std::printf("usage: nesemu_sim <rom> [warmup] [move_frames]\n");
    return 2;
  }

  auto machine = nesemu::Machine::load_rom(read_file(rom_path));
  if (!machine) {
    std::printf("load failed\n");
    return 1;
  }

  // 1) Wait for title/attract to settle
  for (int i = 0; i < 90; ++i) {
    machine->set_controller_state(0, 0);
    machine->run_until_frame();
  }
  // 2) Press Start to enter game
  for (int i = 0; i < 8; ++i) {
    machine->set_controller_state(0, 0x08);
    machine->run_until_frame();
  }
  // 3) Wait for level load
  for (int i = 0; i < static_cast<int>(warmup); ++i) {
    machine->set_controller_state(0, 0);
    machine->run_until_frame();
  }
  std::printf("after start: PC=%04X pal0=%02X\n", machine->cpu().pc(),
              machine->ppu().palette_peek(0));

  nesemu::Framebuffer prev = machine->framebuffer();
  // Hold Right (+ Start occasionally not needed for in-level)
  int left_events = 0;
  int left_px = 0;
  int full_px = 0;
  int frames = 0;
  int shift_sum = 0;
  int shift_abs_sum = 0;
  int shift_flips = 0;
  int prev_shift = 0;
  const u64 c0 = machine->cpu_cycles();
  auto wall_ms = []() -> double {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return 1000.0 * static_cast<double>(c.QuadPart) / static_cast<double>(f.QuadPart);
#else
    return 0;
#endif
  };
  const double t0 = wall_ms();

  for (int i = 0; i < move_frames; ++i) {
    machine->set_controller_state(0, 0x81);
    machine->run_until_frame();
    const auto& fb = machine->framebuffer();
    const int lu = left_unstable(prev, fb, 120);
    const int fu = full_unstable(prev, fb);
    const int hs = hshift_row(prev, fb, 140);
    shift_sum += hs;
    shift_abs_sum += (hs < 0 ? -hs : hs);
    if (i > 0 && hs != prev_shift && (hs == 1 || hs == -1 || hs == 0)) {
      if ((hs == 0 && prev_shift != 0) || (hs != 0 && prev_shift == 0) ||
          (hs == -prev_shift && hs != 0)) {
        ++shift_flips;
      }
    }
    prev_shift = hs;
    if (lu > 0) {
      ++left_events;
      left_px += lu;
    }
    full_px += fu;
    ++frames;
    prev = fb;
    if (i < 12 || i % 30 == 0) {
      std::printf("move f%03d left=%2d full=%5d hshift=%+d pal0=%02X mode=%02X\n", i, lu, fu, hs,
                  fb.pixels[0], machine->bus().peek(0x0770));
    }
  }

  const u64 c1 = machine->cpu_cycles();
  const double t1 = wall_ms();
  std::printf("summary frames=%d left_events=%d avg_left_px=%.2f avg_full_px=%.0f\n", frames,
              left_events,
              frames ? static_cast<double>(left_px) / frames : 0.0,
              frames ? static_cast<double>(full_px) / frames : 0.0);
  std::printf("hshift_sum=%d abs=%d flips=%d avg=%.2f\n", shift_sum, shift_abs_sum, shift_flips,
              frames ? static_cast<double>(shift_sum) / frames : 0.0);
  std::printf("cpu_cycles_per_frame=%.1f emu_fps=%.1f\n",
              frames ? static_cast<double>(c1 - c0) / frames : 0.0,
              (t1 > t0) ? frames * 1000.0 / (t1 - t0) : 0.0);

  // Static control: release input, measure "should be quiet" instability
  for (int i = 0; i < 30; ++i) {
    machine->set_controller_state(0, 0);
    machine->run_until_frame();
  }
  prev = machine->framebuffer();
  int sleft = 0, sfull = 0;
  for (int i = 0; i < 60; ++i) {
    machine->set_controller_state(0, 0);
    machine->run_until_frame();
    const auto& fb = machine->framebuffer();
    sleft += left_unstable(prev, fb, 120);
    sfull += full_unstable(prev, fb);
    prev = fb;
  }
  std::printf("static left_px_sum=%d full_px_sum=%d (60 frames)\n", sleft, sfull);
  return 0;
}
