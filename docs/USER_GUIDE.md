# nesemu 使用手册

NTSC NES / Famicom 模拟器首版使用说明。  
版本：0.1.0 ｜ 平台：Windows（核心库可跨平台）

---

## 1. 简介

nesemu 是一个可玩、可调试的 NES 模拟器，包含：

| 组件 | 说明 |
|---|---|
| **桌面端** `nesemu_desktop` | SDL3 窗口 + 画面 / 音频 / 手柄 + ImGui 调试器 |
| **无头端** `nesemu_headless` | 命令行跑 ROM、截图、帧哈希、音频统计 |
| **测试工具** | `nesemu_nestest` / `nesemu_rom_test` / `nesemu_frame_test` / `nesemu_tests` |

**已支持**
- NTSC 时序
- iNES 与 NES 2.0 ROM 头
- Mapper 0 (NROM)、1 (MMC1)、2 (UxROM)、3 (CNROM)、4 (MMC3)、7 (AxROM)
- 标准双手柄
- 2A03 CPU、2C02 PPU、基础 APU（脉冲 / 三角 / 噪声 / DMC）
- 电池 SRAM、即时存档、倒带

**暂不支持**
- PAL / Dendy、FDS、VS System、PlayChoice
- 光枪等特殊外设、扩展音频
- Mapper 9 / 10 / 11 / 66 / 71 / 206 等（后续扩展）

---

## 2. 安装与构建

### 2.1 环境要求

- CMake 3.28+
- Ninja
- C++20 编译器（MSVC 或 MinGW g++）
- 可选：Git（用于拉取 SDL3 / ImGui / 测试 ROM）

### 2.2 仅核心 + 测试（推荐先跑这个）

```powershell
cd nesemu
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

产物在 `build/dev/`：

| 文件 | 用途 |
|---|---|
| `nesemu_headless.exe` | 命令行运行 ROM |
| `nesemu_tests.exe` | 单元测试 |
| `nesemu_nestest.exe` | CPU 金标 trace |
| `nesemu_rom_test.exe` | 测试 ROM runner |
| `nesemu_frame_test.exe` | 帧哈希 runner |
| `nesemu_debug.exe` | 运行后打印状态快照 |

### 2.3 桌面版（Debug）

```powershell
cmake --preset desktop
cmake --build --preset desktop
```

产物：`build/desktop/nesemu_desktop.exe`  
首次配置会通过 FetchContent 下载 SDL3 与 Dear ImGui，耗时较长属正常现象。

### 2.4 发布打包

```powershell
cmake --preset desktop-release
cmake --build --preset desktop-release
cmake --build build/desktop-release --target package
```

得到 `nesemu-0.1.0-windows-x64.zip`。

### 2.5 外部测试 ROM（可选）

需要 [nes-test-roms](https://github.com/christopherpow/nes-test-roms)：

```powershell
git clone --depth 1 https://github.com/christopherpow/nes-test-roms.git
cmake --preset dev -DNESEMU_TEST_ROM_DIR=<nes-test-roms 路径>
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

当前门槛状态：

```
nestest_trace       8991/8991
ppu_vbl_nmi         10/10
apu_test            8/8
mmc3_irq_tests      通过（1.Clocking）
```

---

## 3. 桌面端使用

### 3.1 启动

```powershell
# 命令行指定 ROM
.\build\desktop\nesemu_desktop.exe "路径\游戏.nes"

# 或启动后拖拽 ROM 到窗口
.\build\desktop\nesemu_desktop.exe
```

支持 `.nes` 及常见 iNES 镜像；也可直接把文件拖进窗口加载。

### 3.2 键盘操作

| 按键 | 功能 |
|---|---|
| 方向键 | 十字键 |
| `X` 或 `Y` | A 键 |
| `Z` 或 `A` | B 键 |
| `Enter` | Start |
| `Right Shift` | Select |
| `Esc` | 暂停 / 继续 |
| `F5` | 即时存档 |
| `F9` | 即时读档 |
| `Backspace` | 倒带（回退历史快照） |

### 3.3 菜单

- **File**：打开 ROM、重置、退出  
- **Emulation**：暂停、存档 / 读档  
- **View**：
  - **Debugger** — 默认关闭。打开后才做断点/单步/反汇编等高开销操作
  - **Integer scale** — 整数倍缩放
  - **Show FPS** — 左上角显示帧率（默认开，开销很小，Release 也启用）

窗口默认按 **4:3** 完整显示 256×240 画面，不会裁切；可随意拉伸窗口。

### 3.4 存档相关

| 类型 | 位置 | 说明 |
|---|---|---|
| **电池 SRAM** | ROM 同目录 `游戏.sav` | 退出时写入；启动时自动读取 |
| **即时存档** | ROM 同目录 `游戏.sav.state` | `F5` 保存，`F9` 读取 |
| **倒带** | 内存快照 | 按住 `Backspace` 回退；有限张数，防内存膨胀 |

**建议**：电池存档使用「临时文件 + 改名」写入，崩溃时不易损坏旧档。

### 3.5 手柄

支持 SDL 识别到的手柄 / Gamepad。键盘映射始终可用；若接了手柄，系统会一并接收输入。

---

## 4. 无头端与命令行工具

### 4.1 `nesemu_headless` — 冒烟运行

```text
nesemu_headless <rom> [帧数] [截图.bmp] [autoplay]
```

示例：

```powershell
.\build\dev\nesemu_headless.exe game.nes 120 shot.bmp
.\build\dev\nesemu_headless.exe game.nes 300 out.bmp autoplay
```

输出内容：

- ROM 格式（iNES / NES 2.0）、Mapper、PRG / CHR 大小
- 运行帧数、最终 PC
- 确定性帧哈希
- 音频采样数、峰值、RMS
- 非零像素数（画面是否为空）
- 可选 BMP 截图

`autoplay` 会自动按一下 Start，便于进标题 / 开始画面。

### 4.2 `nesemu_debug` — 状态快照

```powershell
.\build\dev\nesemu_debug.exe game.nes 180
```

跑指定帧数后打印：CPU 寄存器、PPU 状态、调色板、OAM、零页、nametable 采样、画面颜色直方图。适合排查「黑屏 / 不进游戏」。

### 4.3 `nesemu_nestest` — CPU 金标

```powershell
.\build\dev\nesemu_nestest.exe nestest.nes nestest.log 8991
```

逐条比对 PC / A / X / Y / P / SP / 周期数。官方区应 **8991/8991**。

### 4.4 `nesemu_rom_test` — 测试 ROM

```powershell
.\build\dev\nesemu_rom_test.exe test.nes 300
```

兼容两种结果协议：

1. **Blargg** `$6000` + `$6001..$6003 = DE B0 61`
2. **mmc3_irq_tests** 零页 `$F8`（1 = 通过，其它 = 失败码）

### 4.5 `nesemu_frame_test` — 帧哈希

```powershell
.\build\dev\nesemu_frame_test.exe game.nes 1200 [期望哈希hex]
```

跑固定帧数输出帧哈希；给出期望值时会比对，适合回归测试。

### 4.6 `nesemu_tests` — 单元测试

```powershell
.\build\dev\nesemu_tests.exe
```

---

## 5. 调试器（桌面端）

View → Debugger 打开面板：

| 功能 | 说明 |
|---|---|
| 寄存器 | PC / A / X / Y / SP / P、周期、扫描线、dot、帧号 |
| 运行控制 | Pause / Resume / Step / Step SL / Step Frame |
| 断点 | 输入十六进制地址，Add BP；列表可删除 |
| 反汇编 | 从当前 PC 起 16 条（使用 `peek`，无副作用） |
| 内存 | `$0000` 起 16×16 hex 视图 |

**原则**：调试器查看内存 / 反汇编不改变模拟状态（`peek` 无副作用）。

---

## 6. 游戏兼容性提示

| 情况 | 建议 |
|---|---|
| 黑屏但有声音 | 多半在等 NMI / 渲染开关；用 `nesemu_debug` 看 `mask` 是否为 `$1E` |
| 标题花屏 / 不进游戏 | 检查 Mapper 是否受支持；MMC3 需 IRQ |
| 没声音 | 确认系统默认输出设备；APU 采样率 44100 Hz |
| 存档丢失 | 确认 ROM 目录可写；`.sav` 在 ROM 同目录 |
| 想验证模拟正确性 | 跑 nestest + 外部测试 ROM（见 §2.5） |

**已知可运行**
- 任天堂《超级马里奥兄弟》（NROM）— 标题画面正常
- F1 赛车类 NROM — 画面正常
- 依赖 MMC3 IRQ 精确时序的部分游戏逻辑仍可能有细节差异

---

## 7. 目录结构（开发向）

```text
include/nesemu/     对外 API
src/core/           CPU / PPU / APU / Bus / Mapper / Machine
src/desktop/        SDL3 + ImGui 前端
src/headless/       CLI 工具
tests/              单元测试与 ROM runner
docs/               技术文档
```

核心库不依赖 SDL / 文件对话框 / 真实时间，便于复现与无头测试。

---

## 8. 常见问题

**Q：启动桌面版报 SDL 错误？**  
A：确认是从 `build/desktop/` 运行完整 exe；首次构建需联网拉 SDL3。

**Q：如何重置游戏？**  
A：菜单 File → Reset，或重新加载 ROM。

**Q：倒带回退太多怎么办？**  
A：倒带快照数量有限；可 `F9` 读最近即时存档，或重新加载。

**Q：支持 `.zip` 里的 ROM 吗？**  
A：首版不支持，请解压出 `.nes` 后加载。

**Q：商业 ROM 能放进仓库吗？**  
A：不能。请自备 ROM；仓库只保留开源测试 ROM 的配置方式。

---

## 9. 快捷速查

```text
打开游戏     nesemu_desktop.exe 游戏.nes
暂停         Esc
存 / 读档    F5 / F9
倒带         按住 Backspace
A / B        X或Y / Z或A
Start/Select Enter / RightShift
无头跑图     nesemu_headless game.nes 120 shot.bmp
跑测试       ctest --preset dev --output-on-failure
```

祝你玩得开心。遇到兼容性问题，可用 `nesemu_debug` 导出一帧状态，方便继续排查。
