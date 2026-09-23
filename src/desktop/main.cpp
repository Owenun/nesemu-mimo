// SDL3 + Dear ImGui desktop host for nesemu.
#include "audio.hpp"
#include "nesemu/machine.hpp"
#include "palette.hpp"

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
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

using nesemu::u16;
using nesemu::u32;
using nesemu::u64;

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

bool write_file(const std::string& path, const nesemu::ByteBuffer& data) {
  const std::string tmp = path + ".tmp";
  {
    std::ofstream f(to_wide(tmp).c_str(), std::ios::binary);
    if (!f) {
      return false;
    }
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  }
  const std::wstring wtmp = to_wide(tmp);
  const std::wstring wpath = to_wide(path);
  _wremove(wtmp.c_str());
  return _wrename(wtmp.c_str(), wpath.c_str()) == 0;
}
#else
nesemu::ByteBuffer read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return {};
  }
  return nesemu::ByteBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool write_file(const std::string& path, const nesemu::ByteBuffer& data) {
  const std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary);
    if (!f) {
      return false;
    }
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  }
  std::remove(path.c_str());
  return std::rename(tmp.c_str(), path.c_str()) == 0;
}
#endif

std::string sav_path_for(const std::string& rom_path) {
  std::string p = rom_path;
  const std::size_t dot = p.find_last_of('.');
  if (dot != std::string::npos) {
    p = p.substr(0, dot);
  }
  return p + ".sav";
}

struct RewindSnapshot {
  nesemu::ByteBuffer state;
  u64 frame = 0;
};

}  // namespace

static std::function<void(const std::string&)> g_load_rom_cb;

static void open_rom_callback(void* userdata, const char* const* filelist, int filter) {
  (void)userdata;
  (void)filter;
  if (filelist && filelist[0] && g_load_rom_cb) {
    g_load_rom_cb(std::string(filelist[0]));
  }
}

int main(int argc, char** argv) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
    std::printf("SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("nesemu", 960, 743, SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetRenderVSync(renderer, 0);  // own 60Hz pacing
  const double emu_dt_ms = 1000.0 / 60.0;
  double emu_acc_ms = 0.0;
  u64 prev_ticks = SDL_GetTicks();
  SDL_Texture* tex =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                        nesemu::kScreenWidth, nesemu::kScreenHeight);
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

  SDL_AudioSpec spec{};
  spec.format = SDL_AUDIO_F32;
  spec.channels = 1;
  spec.freq = nesemu::kApuSampleRate;
  SDL_AudioStream* audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                                    nullptr, nullptr);
  if (audio) {
    SDL_ResumeAudioStreamDevice(audio);
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  std::unique_ptr<nesemu::Machine> machine;
  std::string rom_path;
  std::deque<RewindSnapshot> rewind;
  bool paused = false;
  bool show_debugger = false;
  bool integer_scale = false;
  bool show_fps = true;
  float fps_ema = 0.0f;
  u64 fps_last_ticks = 0;
  u32 fps_frame_count = 0;
  float fps_value = 0.0f;
  nesemu_desktop::AudioQueue audio_q;
  std::vector<float> tmp_samples(4096);

  auto load_rom_path = [&](const std::string& path) {
    auto bytes = read_file(path);
    if (bytes.empty()) {
      std::printf("failed to read ROM: %s\n", path.c_str());
      return;
    }
    std::string err;
    auto m = nesemu::Machine::load_rom(bytes, &err);
    if (!m) {
      std::printf("load failed: %s\n", err.c_str());
      return;
    }
    machine = std::move(m);
    rom_path = path;
    rewind.clear();
    std::printf("loaded ROM: %s (mapper %u)\n", path.c_str(), machine->cart().mapper_id());
    // Battery SRAM
    const std::string sav = sav_path_for(path);
    auto sav_bytes = read_file(sav);
    if (!sav_bytes.empty()) {
      machine->cart().battery_load(sav_bytes.data(), sav_bytes.size());
    }
  };

#ifdef _WIN32
  // Prefer wide argv so Chinese ROM paths work regardless of ANSI code page.
  {
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv) {
      if (wargc >= 2) {
        load_rom_path(to_utf8(wargv[1]));
      }
      LocalFree(wargv);
    } else if (argc >= 2) {
      g_load_rom_cb = load_rom_path;
    load_rom_path(argv[1]);
    }
  }
#else
  if (argc >= 2) {
    load_rom_path(argv[1]);
  }
#endif

  bool running = true;
  std::vector<u32> frame_pixels(static_cast<std::size_t>(nesemu::kScreenWidth) *
                                static_cast<std::size_t>(nesemu::kScreenHeight));

  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      ImGui_ImplSDL3_ProcessEvent(&ev);
      if (ev.type == SDL_EVENT_QUIT) {
        running = false;
      }
      if (ev.type == SDL_EVENT_DROP_FILE && ev.drop.data) {
        const std::string dropped(ev.drop.data);
        SDL_free(const_cast<void*>(static_cast<const void*>(ev.drop.data)));
        try {
          load_rom_path(dropped);
        } catch (...) {
          std::printf("drop load failed: %s\n", dropped.c_str());
        }
      }
      if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.repeat == false) {
        switch (ev.key.key) {
          case SDLK_ESCAPE:
            if (machine) {
              paused = !paused;
            }
            break;
          case SDLK_F5:
            if (machine) {
              const nesemu::ByteBuffer st = machine->save_state();
              write_file(rom_path + ".state", st);
            }
            break;
          case SDLK_F9:
            if (machine) {
              auto st = read_file(rom_path + ".state");
              if (!st.empty()) {
                machine->load_state(st, nullptr);
              }
            }
            break;
          case SDLK_BACKSPACE:
            if (machine && !rewind.empty()) {
              machine->load_state(rewind.back().state, nullptr);
              rewind.pop_back();
            }
            break;
          default:
            break;
        }
      }
    }

    // Controllers
    nesemu::ControllerState c0;
    if (machine) {
      if (!ImGui::GetIO().WantCaptureKeyboard) {
        const bool* ks = SDL_GetKeyboardState(nullptr);
        c0.set(nesemu::Button::A, ks[SDL_SCANCODE_X] || ks[SDL_SCANCODE_Z]);
        c0.set(nesemu::Button::B, ks[SDL_SCANCODE_Y] || ks[SDL_SCANCODE_A]);
        c0.set(nesemu::Button::Select, ks[SDL_SCANCODE_RSHIFT] || ks[SDL_SCANCODE_BACKSPACE]);
        c0.set(nesemu::Button::Start, ks[SDL_SCANCODE_RETURN]);
        c0.set(nesemu::Button::Up, ks[SDL_SCANCODE_UP]);
        c0.set(nesemu::Button::Down, ks[SDL_SCANCODE_DOWN]);
        c0.set(nesemu::Button::Left, ks[SDL_SCANCODE_LEFT]);
        c0.set(nesemu::Button::Right, ks[SDL_SCANCODE_RIGHT]);
        // Backspace is rewind hotkey on keydown; don't map to Select when debugging.
        c0.set(nesemu::Button::Select, ks[SDL_SCANCODE_RSHIFT]);
      }
      machine->set_controller_buttons(0, c0);
    }

    // Lock emulation to 60 NES frames per second (not display refresh).
    {
      const u64 now_t = SDL_GetTicks();
      double dt = static_cast<double>(now_t - prev_ticks);
      prev_ticks = now_t;
      if (dt < 0) {
        dt = 0;
      }
      if (dt > 100.0) {
        dt = 100.0;  // after pause/hitch
      }
      emu_acc_ms += dt;
      int emu_frames = 0;
      while (emu_acc_ms >= emu_dt_ms && emu_frames < 4) {
        emu_acc_ms -= emu_dt_ms;
        ++emu_frames;
      }
      if (machine && !paused && !machine->debugger().paused()) {
        for (int k = 0; k < emu_frames; ++k) {
          machine->run_until_frame();
          const std::size_t n = machine->apu().pop_samples(tmp_samples.data(), tmp_samples.size());
          if (n > 0 && audio) {
            audio_q.push(tmp_samples.data(), n);
            if (audio_q.size() > 2048) {
              float buf[2048];
              const std::size_t got = audio_q.pop(buf, 2048);
              SDL_PutAudioStreamData(audio, buf, static_cast<int>(got * sizeof(float)));
            }
          }
          if (machine->frame() % 30 == 0) {
            RewindSnapshot snap;
            snap.state = machine->save_state();
            snap.frame = machine->frame();
            rewind.push_back(std::move(snap));
            if (rewind.size() > 120) {
              rewind.pop_front();
            }
          }
        }
      }
      // If we skipped emulation (paused), keep the clock from exploding.
      if (emu_frames == 0 && emu_acc_ms > emu_dt_ms * 3) {
        emu_acc_ms = emu_dt_ms;
      }
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open ROM...")) {
          // SDL3 file dialog
          const SDL_DialogFileFilter filters[] = {{"NES ROM", "nes;nez"}, {"All files", "*"}};
          g_load_rom_cb = load_rom_path;
          SDL_ShowOpenFileDialog(open_rom_callback, nullptr, window, filters, 1, nullptr, true);
        }
        if (ImGui::MenuItem("Reset") && machine) {
          machine->reset();
        }
        if (ImGui::MenuItem("Quit")) {
          running = false;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Emulation")) {
        ImGui::MenuItem("Pause", "Esc", &paused);
        if (ImGui::MenuItem("Save State", "F5") && machine) {
          write_file(rom_path + ".state", machine->save_state());
        }
        if (ImGui::MenuItem("Load State", "F9") && machine) {
          auto st = read_file(rom_path + ".state");
          if (!st.empty()) {
            machine->load_state(st, nullptr);
          }
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Debugger", nullptr, &show_debugger);
        ImGui::MenuItem("Integer scale", nullptr, &integer_scale);
        ImGui::MenuItem("Show FPS", nullptr, &show_fps);
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    if (machine) {
      auto& dbg = machine->debugger();
      if (show_debugger) {
        ImGui::SetNextWindowSize(ImVec2(420, 720), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debugger");
        ImGui::Text("PC=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X",
                    machine->cpu().pc(), machine->cpu().a(), machine->cpu().x(), machine->cpu().y(),
                    machine->cpu().sp(), machine->cpu().p());
        ImGui::Text("CYC=%llu  SL=%u  DOT=%u  FRAME=%llu",
                    static_cast<unsigned long long>(machine->cpu().cycles()),
                    machine->ppu().scanline(), machine->ppu().dot(),
                    static_cast<unsigned long long>(machine->ppu().frame()));
        if (ImGui::Button("Pause")) {
          dbg.request_pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Resume")) {
          dbg.request_resume();
          paused = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
          machine->step_instruction();
        }
        ImGui::SameLine();
        if (ImGui::Button("Step SL")) {
          machine->step_scanline();
        }
        ImGui::SameLine();
        if (ImGui::Button("Step Frame")) {
          machine->step_frame();
        }

        static u16 bp_addr = 0x8000;
        ImGui::InputScalar("BP addr", ImGuiDataType_U16, &bp_addr, nullptr, nullptr, "%04X",
                           ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::Button("Add BP")) {
          dbg.add_breakpoint(bp_addr);
        }
        for (std::size_t i = 0; i < dbg.breakpoints().size(); ++i) {
          ImGui::Text("BP $%04X", dbg.breakpoints()[i].addr);
          ImGui::SameLine();
          ImGui::PushID(static_cast<int>(i));
          if (ImGui::SmallButton("x")) {
            dbg.remove_breakpoint(i);
            ImGui::PopID();
            break;
          }
          ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::Text("Disassembly");
        u16 addr = machine->cpu().pc();
        for (int i = 0; i < 16; ++i) {
          std::string dis;
          const std::size_t sz = dbg.disassemble(addr, dis);
          ImGui::Text("%04X  %s", addr, dis.c_str());
          addr = static_cast<u16>(addr + sz);
        }

        ImGui::Separator();
        ImGui::Text("Memory $0000");
        for (int row = 0; row < 16; ++row) {
          char line[80];
          int n = std::snprintf(line, sizeof(line), "%04X:", row * 16);
          for (int c = 0; c < 16; ++c) {
            n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %02X",
                               machine->bus().peek(static_cast<std::uint16_t>(row * 16 + c)));
          }
          ImGui::TextUnformatted(line);
        }
        ImGui::End();
      }
    }

    // Present
    if (machine) {
      const auto& fb = machine->framebuffer();
      const auto& pal = nesemu_desktop::nes_palette_rgba();
      for (int i = 0; i < nesemu::kScreenWidth * nesemu::kScreenHeight; ++i) {
        frame_pixels[i] = pal[fb.pixels[static_cast<std::size_t>(i)] & 0x3F];
      }
      SDL_UpdateTexture(tex, nullptr, frame_pixels.data(), nesemu::kScreenWidth * 4);
    }

    SDL_SetRenderDrawColor(renderer, 16, 16, 20, 255);
    SDL_RenderClear(renderer);

    int ww = 0, wh = 0;
    SDL_GetWindowSize(window, &ww, &wh);
    // Fit 4:3 game area inside the window (do not overflow/crop).
    const float menu_h = 22.0f;
    const float avail_w = static_cast<float>(ww);
    const float avail_h = (static_cast<float>(wh) > menu_h) ? static_cast<float>(wh) - menu_h : 1.0f;
    // NES 256x240 shown at 4:3 → dest aspect w/h = 4/3
    float dst_h = avail_h;
    float dst_w = dst_h * (4.0f / 3.0f);
    if (dst_w > avail_w) {
      dst_w = avail_w;
      dst_h = dst_w * (3.0f / 4.0f);
    }
    if (integer_scale) {
      // Integer scale of 256x240, then apply 4:3 horizontally.
      float s = std::floor(std::min(avail_w / 320.0f, avail_h / 240.0f));
      if (s < 1.0f) {
        s = 1.0f;
      }
      dst_h = 240.0f * s;
      dst_w = 320.0f * s;
      if (dst_w > avail_w) {
        dst_w = avail_w;
        dst_h = dst_w * (3.0f / 4.0f);
      }
      if (dst_h > avail_h) {
        dst_h = avail_h;
        dst_w = dst_h * (4.0f / 3.0f);
      }
    }
    SDL_FRect dst{0, 0, dst_w, dst_h};
    dst.x = (avail_w - dst_w) * 0.5f;
    dst.y = menu_h + (avail_h - dst_h) * 0.5f;
    // Snap to whole pixels: fractional dest rects make the top/bottom
    // scanlines resample unevenly while the camera scrolls (edge shimmer).
    dst.x = std::floor(dst.x);
    dst.y = std::floor(dst.y);
    dst.w = std::floor(dst.w);
    dst.h = std::floor(dst.h);
    if (dst.w < 1.0f) dst.w = 1.0f;
    if (dst.h < 1.0f) dst.h = 1.0f;
    if (machine) {
      SDL_RenderTexture(renderer, tex, nullptr, &dst);
    }

    // Cheap FPS counter (one timestamp + EMA per present).
    {
      const u64 now = SDL_GetTicks();
      if (fps_last_ticks == 0) {
        fps_last_ticks = now;
      }
      ++fps_frame_count;
      if (now - fps_last_ticks >= 500) {
        const float sec = static_cast<float>(now - fps_last_ticks) / 1000.0f;
        fps_value = (sec > 0.0f) ? static_cast<float>(fps_frame_count) / sec : 0.0f;
        fps_ema = (fps_ema == 0.0f) ? fps_value : (fps_ema * 0.7f + fps_value * 0.3f);
        fps_last_ticks = now;
        fps_frame_count = 0;
      }
      if (show_fps) {
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::SetNextWindowPos(ImVec2(10, 28), ImGuiCond_Always);
        if (ImGui::Begin("##fps", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
          ImGui::Text("UI %.0f | NES 60Hz", fps_ema > 0.0f ? fps_ema : fps_value);
        }
        ImGui::End();
      }
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  // Battery save on exit
  if (machine && machine->cart().has_battery()) {
    nesemu::ByteBuffer data(machine->cart().battery_data(),
                            machine->cart().battery_data() + machine->cart().battery_size());
    write_file(sav_path_for(rom_path), data);
  }

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  if (audio) {
    SDL_DestroyAudioStream(audio);
  }
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
