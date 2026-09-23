# nesemu 实现报告

NTSC NES / Famicom 模拟器 — 架构、原理、构建、缺陷修复与测试说明。  
版本：0.1.0 ｜ 平台：Windows（核心库可移植）｜ 仓库：nesemu-mimo

---

## 1. 项目概述

目标：可玩、可调试、可回归的 NTSC NES 模拟器首版。

| 能力 | 状态 |
|---|---|
| NTSC 时序 | 已实现（约 29780.5 CPU 周期/帧） |
| iNES / NES 2.0 头 | 已实现 |
| Mapper 0 / 1 / 2 / 3 / 4 / 7 | 已实现（NROM / MMC1 / UxROM / CNROM / MMC3 / AxROM） |
| 2A03 CPU（含常用 unofficial） | 已实现，nestest 8991/8991 |
| 2C02 PPU（点级） | 已实现，ppu_vbl_nmi 10/10 |
| APU（脉冲/三角/噪声/DMC） | 已实现，apu_test 8/8 |
| 双手柄 / 电池 SRAM / 即时存档 / 倒带 | 已实现 |
| SDL3 桌面端 + ImGui 调试器 | 已实现 |
| 无头测试工具链 | 已实现 |

未列入首版：PAL/Dendy、FDS、VS、光枪、扩展音频、更多 Mapper、联机。

---

## 2. 总体架构

「无平台核心 + 多宿主前端」：

```text
include/nesemu/     稳定对外 API
src/core/           CPU / PPU / APU / Bus / Mapper / Machine / State / Debugger
src/desktop/        SDL3 + Dear ImGui 桌面端
src/headless/       CLI：headless / debug / sim
tests/              单元测试与 nestest / rom / frame runner
docs/               技术文档、使用手册、本报告
```

对象关系：

```text
Host (Desktop / Headless)
        |
     Machine          ← 时序总控
    /  |  |  \
  Cpu Bus Ppu Apu
        |    |
     Mapper  Cartridge
        |
    Debugger
```

核心约束：

- `Cpu` 只通过 `CpuBus` 访存，不感知 PPU/APU/Mapper 类型。
- `Ppu` 只通过 `Mapper` 的 PPU 侧访问 pattern。
- `Mapper` 封装板级行为；CPU/PPU 无 mapper 专用分支。
- `read` 有副作用并推进时钟；`peek` 无副作用（调试器专用）。
- 存档逐字段序列化 + 版本号，禁止 dump 内存结构体。

---

## 3. NES 模拟原理

### 3.1 时序模型（CPU bus cycle 驱动）

每次真实总线读写：

```text
1 CPU bus cycle
  → PPU tick × 3
  → APU clock × 1
  → 采样 NMI / IRQ
  → 处理 DMA 请求
```

因此 dummy read、page crossing、OAM DMA、NMI 边沿、APU frame IRQ、MMC3 IRQ 都在同一时间轴上。  
每帧 CPU 周期数实测 **29780.5**，与 NTSC 相符。

### 3.2 CPU（Ricoh 2A03）

- 指令表：`Operation + AddressMode`；每寻址模式显式做总线访问（含 dummy）。
- 支持 official opcode、常用 unofficial（LAX/SAX/DCP/ISC/SLO/RLA/SRE/RRA 等）。
- 关键细节：JMP 间接页内回绕 bug、RMW 先写原值再写新值、page-crossing dummy read、`PLA/PLP` 四周期、B 标志不进 P 寄存器（仅入栈）。
- 中断：NMI 边沿锁存；IRQ 电平、受 I 位屏蔽；处理程序可用 `RTI` 或 `RTS` 返回（如 MMC3 测试 ROM）。

### 3.3 CPU 总线映射

```text
$0000-$1FFF  2KiB RAM 镜像
$2000-$3FFF  PPU 寄存器（8 字节镜像）
$4000-$4013  APU
$4014        OAM DMA
$4015        APU 状态
$4016/$4017  手柄 / APU frame counter
$4020-$FFFF  卡带 / Mapper
```

`SystemBus` 处理 mirror、open bus、手柄串行协议，并把 `$2000+` 委托给 Machine。

### 3.4 PPU（2C02）

输出 256×240 调色板索引（非 RGB）；颜色转换在前端。

实现要点：

- 可见区逐点渲染；背景按 **每行卷轴快照 + tile 缓存** 解析像素。
- X 卷轴取自 `t` + fine X（不受行尾预取递增污染）；Y 取自 `v`（随 `increment_y` 前进）。
- 精灵求值为「下一条扫描线」准备；支持 8×8 / 8×16、水平/垂直翻转、sprite 0 hit、overflow 粗略实现。
- VBlank：扫描线 241 dot 1 置位；pre-render 清 vblank / sprite0 / overflow。
- `$2002` 读清 vblank 与 write toggle；`$2006/$2007` 地址变化通知 Mapper（MMC3 A12）。
- 奇帧在渲染开启时缩短 pre-render 一格（odd frame skip）。

### 3.5 APU

脉冲 ×2、三角、噪声、DMC、frame counter、非线性混合器。  
采样环形缓冲（约 44.1kHz），宿主只消费采样，**绝不在音频回调里推进模拟器**。

### 3.6 DMA

- **OAM DMA**（`$4014`）：513/514 周期，256 次 CPU read + PPU OAM write。
- **DMC DMA**：APU 请求后由 Machine 供给样本字节并消耗总线周期。

### 3.7 Mapper

统一接口：`cpu_read/write/peek`、`ppu_read/write/peek`、`mirroring`、`irq_line`、`observe_ppu_address`、`save/load_state`。

| ID | 名称 | 要点 |
|---:|---|---|
| 0 | NROM | 16/32KiB PRG，8KiB CHR |
| 1 | MMC1 | 串行移位寄存器，PRG/CHR bank，mirroring |
| 2 | UxROM | 16KiB 可切换 + 固定末 bank |
| 3 | CNROM | 8KiB CHR bank |
| 4 | MMC3 | PRG/CHR bank、A12 扫描线 IRQ |
| 7 | AxROM | 32KiB PRG、单屏 mirroring |

MMC3 IRQ：`$C000/$C001/$E000/$E001` 管理 latch/reload/enable/ack；A12 上升沿计数（含短脉冲滤波）。

### 3.8 存档 / 倒带

- 电池 SRAM → ROM 同目录 `.sav`（临时文件 + rename）。
- 即时存档：magic + version + ROM fingerprint + 各组件字段；`load_state` 事务式回滚。
- 倒带：固定间隔快照、数量上限。

---

## 4. 桌面前端

- SDL3 窗口 / 渲染（最近邻、4:3 完整适配）、键鼠/手柄、音频流、拖放 ROM。
- Dear ImGui：菜单栏、调试器（寄存器、反汇编、内存、断点、单步）、FPS 浮层。
- **帧率语义**：
  - **UI xx**：窗口每秒重绘次数（随显示器/合成器波动，例如 60–144）。
  - **NES 60Hz**：模拟器按 60 游戏帧/秒推进（计时器 pacing，不随 100/120Hz 屏加速）。
- 调试器默认关闭；仅断点/单步/录制 trace 时产生每指令开销。

### 快捷键

方向键移动｜X/Y=A｜Z/A=B｜Enter=Start｜RShift=Select｜Esc=暂停｜F5/F9=存读档｜Backspace=倒带。

---

## 5. 工程与构建

### 5.1 技术栈

- C++20，CMake 3.28+，Ninja，CMake Presets
- SDL3（FetchContent）、Dear ImGui（FetchContent）
- 自研轻量单元测试 + 外部 ROM runner
- CPack ZIP 打包

### 5.2 预设

| Preset | 用途 |
|---|---|
| `dev` | 核心 + 测试 + 无头工具 |
| `desktop` | Debug 桌面端 |
| `desktop-release` | Release + package |

### 5.3 常用命令

```powershell
# 核心 + 单元测试
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure

# 外部测试 ROM（nes-test-roms）
cmake --preset dev -DNESEMU_TEST_ROM_DIR=<nes-test-roms root>
cmake --build --preset dev
ctest --preset dev --output-on-failure

# 桌面端
cmake --preset desktop
cmake --build --preset desktop

# 发布
cmake --preset desktop-release
cmake --build --preset desktop-release
cmake --build build/desktop-release --target package
```

### 5.4 产物

`nesemu_desktop` / `nesemu_headless` / `nesemu_debug` / `nesemu_sim` / `nesemu_nestest` / `nesemu_rom_test` / `nesemu_frame_test` / `nesemu_tests`。

---

## 6. 缺陷修复报告（节选）

| 现象 | 根因 | 修复 |
|---|---|---|
| 桌面端黑屏秒退、无菜单 | 调用 `RenderDrawData` 前未 `ImGui::Render()`，`GetDrawData()` 空指针 | 补 `ImGui::Render()` |
| 中文路径 ROM 打不开 | Windows 窄字符路径 / 误把 ANSI 当 UTF-8 | 宽字符 argv + 宽字符文件 IO |
| nestest 周期少 1 | 隐含寻址缺 dummy cycle | `read8(pc)` dummy |
| nestest P 多 B 位 | B 被写入 P | P 屏蔽 B；入栈时再置 B |
| nestest LSR 不置 Z | 内存 RMW 未 `set_zn` | RMW 后 `set_zn(result)` |
| nestest ZeroPageX 错 | `zp+X` 未加 X | 修正寻址 |
| PPU 全是背景色 | 背景移位/预取相位错误 | 重写为行快照直取 + tile 缓存 |
| SMB 左侧列错位、移动闪动 | 行尾预取递增 `v` 后仍用其 X 画像素 | X 卷轴改从 `t`+fineX 快照 |
| 三 tile 跳块 | FIFO 覆盖「下一块」 | 改为正确快照/缓存模型 |
| 高刷屏移动抖动 | 1 刷新 = 1 NES 帧，100Hz 变 1.6 倍速 | 固定 60Hz pacing |
| MMC3 测试无结果 | 结果在零页 `$F8` 而非 `$6000`；IRQ 被 `in_interrupt_` 卡死 | 兼容 `$F8`；IRQ 允许 RTS 返回 |
| 调试器拖慢 | 每指令反汇编/trace | 默认关闭；hook 按需 |
| UI「帧率低」 | UI FPS≠模拟速度 | 文案改为 `UI xx \| NES 60Hz`；APU 环形缓冲降开销 |

---

## 7. 测试与质量门槛

### 7.1 单元测试（30）

Cartridge / CPU 寻址与标志 / PPU VBlank、背景、fine-X、三 tile、sprite0 / APU / Mapper / MMC3 IRQ / 存档 round-trip / 帧周期 29780±。

### 7.2 外部门槛（nes-test-roms）

```text
nestest_trace       8991/8991
ppu_vbl_nmi         10/10
apu_test            8/8
mmc3_irq_tests      通过（1.Clocking；另 3.A12、6.Rev_B 亦通过）
```

### 7.3 冒烟

无头跑真实 ROM（SMB / 魂斗罗 / Tetris / F1 等）校验 mapper、帧哈希、音频 RMS、非零像素；`nesemu_sim` 自动 Start+右移做卷轴稳定性测量。

---

## 8. 已知限制与后续

1. MMC3：`2.Details`（每帧 241 次 A12）、`4.Scanline_timing` 微时序、Rev A 与 Rev B 互斥用例未全绿。
2. Sprite overflow / OAM decay / DMC-OAM DMA 碰撞为近似。
3. 更多 Mapper、PAL、条件断点、PPU 调可视化待扩展。
4. 桌面端文件对话框、最近 ROM 列表可再产品化。

---

## 9. 文档索引

| 文档 | 内容 |
|---|---|
| `README.md` | 简介与快速开始 |
| `docs/USER_GUIDE.md` | 用户使用手册 |
| `docs/TECHNICAL_IMPLEMENTATION.md` | 原始技术实现指南 |
| `docs/IMPLEMENTATION_REPORT.md` | 本报告 |

---

## 10. 结论

项目已达到「经典 NTSC 游戏可玩 + 可回归」的首版目标：核心无平台依赖、时序与 nestest 对齐、PPU/APU 过公开测试门槛、桌面端可完整游玩马里奥等作品，并具备调试与无头测试能力。后续以兼容性矩阵与 Mapper 扩展为主。


---

## 附录：0.1.x 迭代摘要（2026-09-22）

### 已修复
- Logo 假死：实为等待 Start（BUG_REPORT BR-01）
- 标题底绿条 / 战斗字：MMC3 IRQ 每扫描线计数 + A12 过滤（BR-02/03）
- 拖放 ROM、打开对话框崩溃（BR-04）
- 呈现撕裂：PPU 双缓冲，仅发布完整帧
- 桌面 4:3 缩放目标矩形整像素对齐（BR-06）

### 遗留
- 垂直卷动顶/底偶发串行（BR-05，阶段性接受；未再钳位 increment_y）

### 相关文档
- docs/TEST_REPORT.md
- docs/BUG_REPORT.md
