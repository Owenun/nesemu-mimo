#include "nesemu/machine.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
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
using nesemu::u32;

namespace {

#ifdef _WIN32
static std::wstring to_wide(const std::string& path) {
  if (path.empty()) {
    return {};
  }
  int n = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
  if (n > 0) {
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') {
      w.pop_back();
    }
    return w;
  }
  n = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, nullptr, 0);
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

static std::string to_utf8(const std::wstring& w) {
  if (w.empty()) {
    return {};
  }
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (n <= 0) {
    return {};
  }
  std::string s(static_cast<std::size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
  return s;
}

nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(to_wide(path).c_str(), std::ios::binary);
  if (!f) {
    return {};
  }
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
#else
nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return {};
  }
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
#endif

void write_bmp(const std::string& path, const nesemu::Framebuffer& fb) {
  // 24-bit BMP, 256x240, using a fixed NES palette approx.
  static const u32 kPal[64] = {
      0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
      0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
      0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
      0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
      0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
      0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
      0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
      0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000};

  namespace ne = nesemu;
  const int w = ne::kScreenWidth;
  const int h = ne::kScreenHeight;
  const int row = ((w * 3 + 3) / 4) * 4;
  const int data_size = row * h;
  const int file_size = 54 + data_size;

  std::vector<unsigned char> bmp(static_cast<std::size_t>(file_size), 0);
  auto put32 = [&](int off, u32 v) {
    bmp[static_cast<std::size_t>(off)] = static_cast<unsigned char>(v & 0xFF);
    bmp[static_cast<std::size_t>(off + 1)] = static_cast<unsigned char>((v >> 8) & 0xFF);
    bmp[static_cast<std::size_t>(off + 2)] = static_cast<unsigned char>((v >> 16) & 0xFF);
    bmp[static_cast<std::size_t>(off + 3)] = static_cast<unsigned char>((v >> 24) & 0xFF);
  };
  bmp[0] = 'B';
  bmp[1] = 'M';
  put32(2, static_cast<u32>(file_size));
  put32(10, 54);
  put32(14, 40);
  put32(18, static_cast<u32>(w));
  put32(22, static_cast<u32>(h));
  bmp[26] = 1;
  bmp[28] = 24;
  put32(34, static_cast<u32>(data_size));

  for (int y = 0; y < h; ++y) {
    const int src_y = h - 1 - y;
    for (int x = 0; x < w; ++x) {
      const int i = src_y * w + x;
      const u8 idx = fb.pixels[static_cast<std::size_t>(i)] & 0x3F;
      const u32 c = kPal[idx];
      const int o = 54 + y * row + x * 3;
      bmp[static_cast<std::size_t>(o)] = static_cast<unsigned char>((c >> 16) & 0xFF);
      bmp[static_cast<std::size_t>(o + 1)] = static_cast<unsigned char>((c >> 8) & 0xFF);
      bmp[static_cast<std::size_t>(o + 2)] = static_cast<unsigned char>(c & 0xFF);
    }
  }

#ifdef _WIN32
  int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
  std::wstring wpath;
  if (wlen > 0) {
    wpath.assign(static_cast<std::size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
    if (!wpath.empty() && wpath.back() == L'\0') {
      wpath.pop_back();
    }
  } else {
    wpath.assign(path.begin(), path.end());
  }
  std::ofstream out(wpath.c_str(), std::ios::binary);
#else
  std::ofstream out(path, std::ios::binary);
#endif
  out.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
}

u32 fnv1a_frame(const nesemu::Framebuffer& fb) {
  u32 h = 2166136261u;
  for (int i = 0; i < nesemu::kScreenWidth * nesemu::kScreenHeight; ++i) {
    h ^= fb.pixels[static_cast<std::size_t>(i)];
    h *= 16777619u;
  }
  return h;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  // UTF-8 console output and wide argv for Chinese paths.
  SetConsoleOutputCP(CP_UTF8);
#endif
  std::string rom_path;
  int frames = 60;
  std::string shot_path;
  bool autoplay = false;
#ifdef _WIN32
  {
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv) {
      if (wargc >= 2) rom_path = to_utf8(wargv[1]);
      if (wargc >= 3) frames = static_cast<int>(wcstol(wargv[2], nullptr, 10));
      if (wargc >= 4) shot_path = to_utf8(wargv[3]);
      if (wargc >= 5) autoplay = (wcscmp(wargv[4], L"autoplay") == 0);
      LocalFree(wargv);
    } else if (argc >= 2) {
      rom_path = argv[1];
      if (argc >= 3) frames = std::atoi(argv[2]);
      if (argc >= 4) shot_path = argv[3];
      if (argc >= 5) autoplay = std::strcmp(argv[4], "autoplay") == 0;
    }
  }
#else
  if (argc < 2) {
    std::printf("usage: nesemu_headless <rom> [frames] [screenshot.bmp] [autoplay]\n");
    return 2;
  }
  rom_path = argv[1];
  if (argc >= 3) frames = std::atoi(argv[2]);
  if (argc >= 4) shot_path = argv[3];
  if (argc >= 5) autoplay = std::strcmp(argv[4], "autoplay") == 0;
#endif
  if (rom_path.empty()) {
    std::printf("usage: nesemu_headless <rom> [frames] [screenshot.bmp] [autoplay]\n");
    return 2;
  }

  const auto bytes = read_file(rom_path);
  if (bytes.empty()) {
    std::printf("failed to read ROM: %s\n", rom_path.c_str());
    return 1;
  }

  std::string err;
  auto machine = nesemu::Machine::load_rom(bytes, &err);
  if (!machine) {
    std::printf("load failed: %s\n", err.c_str());
    return 1;
  }

  const auto& h = machine->cart().header();
  std::printf("format: %s\n", h.nes2 ? "NES 2.0" : "iNES");
  std::printf("mapper: %u\n", machine->cart().mapper_id());
  std::printf("prg: %u KiB  chr: %u KiB\n", h.prg_rom_size / 1024, h.chr_rom_size / 1024);

  if (autoplay) {
    machine->set_controller_state(0, 0x08);  // Start
  }

  for (int i = 0; i < frames; ++i) {
    machine->run_until_frame();
    if (machine->cpu().jammed()) {
      std::printf("CPU jammed at frame %d\n", i);
      break;
    }
    if (autoplay) {
      machine->set_controller_state(0, 0);
    }
  }

  const auto& fb = machine->framebuffer();
  const u32 hash = fnv1a_frame(fb);
  std::printf("frames: %d\n", frames);
  std::printf("pc: $%04X\n", machine->cpu().pc());
  std::printf("frame_hash: 0x%08X\n", hash);

  std::size_t nsamples = machine->apu().samples_available();
  float peak = 0.0f;
  double sumsq = 0.0;
  std::vector<float> tmp(nsamples);
  const std::size_t got = machine->apu().pop_samples(tmp.data(), tmp.size());
  for (std::size_t i = 0; i < got; ++i) {
    peak = std::max(peak, std::fabs(tmp[i]));
    sumsq += static_cast<double>(tmp[i]) * tmp[i];
  }
  const float rms = got ? static_cast<float>(std::sqrt(sumsq / got)) : 0.0f;
  std::printf("audio_samples: %zu peak: %.4f rms: %.4f\n", got, peak, rms);

  // Non-empty screen check: count distinct colors
  int nonzero = 0;
  for (int i = 0; i < nesemu::kScreenWidth * nesemu::kScreenHeight; ++i) {
    if (fb.pixels[static_cast<std::size_t>(i)] != 0) {
      ++nonzero;
    }
  }
  std::printf("nonzero_pixels: %d\n", nonzero);

  if (!shot_path.empty()) {
    write_bmp(shot_path, fb);
    std::printf("screenshot: %s\n", shot_path.c_str());
  }
  return machine->cpu().jammed() ? 1 : 0;
}
