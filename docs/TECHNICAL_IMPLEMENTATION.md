# NES Emulator Technical Implementation Guide

工程说明：解释本项目的技术栈、总体架构、NES 模拟原理、
模块边界、实现顺序、测试策略和质量门槛。

目标是复现一个可玩的、可调试的 NTSC NES/Famicom 模拟器首版，支持
Mapper 0、1、2、3、4、7，具备 SDL3 桌面端、音频、输入、存档、倒带、
无头测试器和集成调试器。

## 1. 技术栈

核心语言：C++20。

构建系统：CMake 3.28+，Ninja，CMake Presets。

桌面端：SDL3 负责窗口、输入、音频、文件对话框和渲染；Dear ImGui 负责
调试器 UI。

测试：自研轻量单元测试框架，加上 headless ROM runner、`nestest` trace runner、
Blargg/nes-test-roms 风格的 ROM 测试 runner、确定性帧哈希 runner。

打包：CPack ZIP，Windows 桌面版默认生成 `nesemu-0.1.0-windows-x64.zip`。

工程质量：warning-as-error、可选 ASan/UBSan、可选 libFuzzer 卡带解析入口、
GitHub Actions 三平台核心 CI 和 Windows 桌面打包 CI。

第三方依赖固定版本：

| 依赖 | 用途 | 获取方式 |
|---|---|---|
| SDL3 3.4.16 | 桌面窗口、音频、输入、渲染、文件对话框 | CMake FetchContent |
| Dear ImGui 1.92.9b | 调试器 UI | CMake FetchContent |

核心库不能依赖 SDL、ImGui、文件对话框或真实时间。所有平台相关内容只能出现在
`src/desktop`、`src/headless` 或测试工具里。

## 2. 产品范围

首版目标是“经典 NTSC NES 游戏可玩”，不是一次性覆盖全 NES 生态。

首版支持：

- NTSC 时序。
- iNES 和 NES 2.0 ROM 头解析。
- Mapper 0、1、2、3、4、7。
- 标准双手柄。
- 2A03 CPU、2C02 PPU、基础 APU。
- 电池 SRAM、即时存档、倒带。
- 桌面运行、无头测试、调试器。

首版不支持：

- PAL/Dendy 完整时钟。
- FDS、VS System、PlayChoice。
- 光枪、特殊外设。
- 扩展音频。
- Mapper 9、10、11、66、71、206 等后续兼容性阶段。
- 网络联机。

这个范围控制很重要。NES 模拟器很容易因为“顺手支持更多东西”而把工程复杂度
提前爆炸。先打穿 NROM、CNROM、MMC1、UxROM、MMC3 和 AxROM 这条主线，
再扩展 Mapper。

## 3. 总体架构

项目采用“无平台核心 + 多宿主前端”的结构。

```text
include/nesemu/       对外核心 API 和稳定数据类型
src/core/             CPU、PPU、APU、Bus、Mapper、Machine、State、Debugger
src/headless/         命令行 ROM runner
src/desktop/          SDL3 + Dear ImGui 桌面端
tests/                单元测试和外部 ROM 测试 runner
tests/fuzz/           可选 libFuzzer 入口
docs/                 工程计划、兼容性报告和技术文档
```

核心对象关系：

```text
Desktop / Headless Host
        |
        v
Machine
  |-- Cpu
  |-- SystemBus
  |     |-- CPU RAM
  |     |-- PPU registers
  |     |-- APU / controller I/O
  |     `-- Mapper CPU side
  |-- Ppu
  |     `-- Mapper PPU side
  |-- Apu
  |-- Cartridge
  |-- Mapper
  `-- Debugger
```

核心原则：

- `Machine` 是时序总控，负责把 CPU bus cycle、PPU dot、APU clock、DMA 和 IRQ 串起来。
- `Cpu` 不知道 PPU、APU、Mapper 的具体类型，只通过 `CpuBus` 读写。
- `Ppu` 不知道具体 Mapper，只通过 `Mapper` 的 PPU 接口读写 pattern table。
- `Mapper` 封装卡带板级行为，CPU/PPU 里不能出现 mapper-specific 分支。
- 调试器 peek 必须无副作用，不能因为打开内存窗口就读掉 PPU 状态或手柄移位位。
- 存档必须字段化、版本化，不能 dump C++ struct 原始内存。

## 4. 核心时序模型

本项目采用 CPU bus cycle 驱动模型。CPU 每发生一次真实总线访问，就调用
`SystemBus::read` 或 `SystemBus::write`，Bus 在访问前通知 `Machine::on_cpu_bus_cycle()`。

每个 CPU bus cycle 内做：

```text
1 CPU bus cycle
  -> PPU tick 3 次
  -> APU clock 1 次
  -> 采样 NMI/IRQ 线
```

这样能让 CPU dummy read、page crossing、DMA、PPU NMI 边沿、APU frame IRQ、
Mapper IRQ 全都落在同一个时间轴上。

关键点：

- CPU 指令不是“按 opcode 一次性加 cycles”，而是在每个微操作里真的读写总线。
- OAM DMA 和 DMC DMA 通过 CPU dummy cycle/read/write 消耗总线时间。
- PPU 每 tick 走一个 dot，NTSC 下每 CPU cycle 走三个 dot。
- APU 在 CPU cycle 粒度推进，内部再处理 frame counter、channel timer 和 DMC。
- `Machine::run_until_frame()` 只循环执行 CPU 指令，直到 PPU 产生 frame-ready。

## 5. ROM 与 Cartridge

`Cartridge::from_bytes` 负责从 ROM bytes 构造卡带对象。

实现要求：

- 校验 `NES<EOF>` 签名。
- 支持 iNES 和 NES 2.0 mapper/submapper/size 字段。
- 使用 checked arithmetic，避免恶意 header 造成整数溢出。
- 正确处理 trainer、PRG ROM、CHR ROM、CHR RAM、PRG RAM、NVRAM。
- 解析 mirroring、battery、timing、console type。
- 对 VS System 和 PlayChoice 给出明确 unsupported 错误。
- 提供 ROM fingerprint，用于 save state 和兼容性记录。

卡带只保存数据和元信息。银行切换、IRQ、PRG/CHR 映射由 Mapper 负责。

## 6. Mapper 设计

Mapper 是 CPU/PPU 与卡带板之间的接口。

接口分为：

- `cpu_read/cpu_write/cpu_peek`
- `ppu_read/ppu_write/ppu_peek`
- `mirroring`
- `irq_line`
- `clock_ppu`
- `observe_ppu_address`
- `save_state/load_state`

`peek` 是调试器和反汇编专用，必须无副作用。

首版实现：

| Mapper | 常见名称 | 作用 |
|---:|---|---|
| 0 | NROM | 无银行切换，16/32 KiB PRG，8 KiB CHR |
| 1 | MMC1 | 串行寄存器，PRG/CHR banking，mirroring |
| 2 | UxROM | 16 KiB PRG banking，固定末 bank |
| 3 | CNROM | 8 KiB CHR banking |
| 4 | MMC3 | PRG/CHR banking，A12 scanline IRQ |
| 7 | AxROM | 32 KiB PRG banking，single-screen mirroring |

MMC3 的关键实现：

- PRG 4 个 8 KiB slot，支持 PRG mode 反转。
- CHR 8 个 1 KiB slot，支持 CHR A12 inversion。
- `$C000/$C001/$E000/$E001` 管理 IRQ latch、reload、enable、ack。
- 观察 PPU A12 上升沿，并用滤波门槛抑制短 pattern-fetch 脉冲。
- `$2006/$2007` 引起的 PPU 地址变化也必须被 Mapper 观察，否则 MMC3 A12 测试会失败。

## 7. CPU 实现

CPU 目标是模拟 Ricoh 2A03 的 6502 子集和常见 unofficial opcode。

实现方式：

- opcode table 描述 `Operation + AddressMode`。
- 每个 addressing mode 显式执行读写和 dummy access。
- 每次 bus access 都推进全机时钟。
- 状态包含 `A/X/Y/PC/SP/P/cycles/jammed`。
- 支持 reset、NMI、IRQ、BRK、RTI、JSR/RTS、stack、page crossing。
- 实现 indirect JMP wraparound bug。
- 实现 read-modify-write 的 dummy write 行为。
- 对稳定 unofficial opcode 提供兼容实现。

不要把 CPU 写成“opcode -> cycles += N”的解释器。那样早期能跑小程序，但会在
DMA、NMI timing、PPUSTATUS race、Mapper IRQ 上持续出错。

## 8. CPU Bus

CPU 地址空间：

```text
$0000-$1FFF  2 KiB internal RAM mirrored
$2000-$3FFF  PPU registers mirrored every 8 bytes
$4000-$4013  APU registers
$4014        OAM DMA
$4015        APU status
$4016-$4017  controllers / APU frame counter
$4020-$FFFF  cartridge / mapper
```

Bus 职责：

- 处理 RAM/register mirror。
- 维护 CPU open bus。
- 区分 `read` 和 `peek`。
- 手柄 `$4016/$4017` 串行读取。
- 把 PPU/APU I/O 委托回 `Machine`。
- 把 `$4020+` 委托给 Mapper。

`SystemBus::read/write` 负责推进 CPU bus cycle；`peek` 不推进时间。

## 9. PPU 实现

PPU 采用 dot-level pipeline，而不是 frame-level tile renderer。

核心状态：

- `v`：current VRAM address。
- `t`：temporary VRAM address。
- `x`：fine X scroll。
- `w`：write toggle。
- `control/mask/status`。
- background shift registers。
- sprite evaluation buffers。
- nametable RAM、palette RAM、OAM。

每个 PPU dot 做：

- visible dots 渲染一个像素。
- background fetch 按 nametable、attribute、pattern low、pattern high 循环。
- dots 1-256 和 321-336 移动背景 shifter。
- dot 256 垂直滚动。
- dot 257 水平复制并做 sprite evaluation。
- pre-render scanline dots 280-304 复制垂直滚动。
- scanline 241 dot 1 设置 VBlank。
- pre-render dot 1 清除 VBlank、sprite zero、overflow。
- odd frame skip 在启用渲染时跳过 pre-render 的一个 dot。

PPU 输出不是 RGB，而是 `256x240` 的 NES palette index/emphasis 值。颜色转换是前端或 headless 截图工具的职责。

## 10. APU 实现

APU 包含：

- 两个 pulse channel。
- triangle channel。
- noise channel。
- DMC channel。
- frame counter。
- nonlinear mixer。
- deterministic resampler / sample ring。

关键细节：

- `$4017` frame counter 写入有 CPU cycle 对齐延迟。
- 4-step mode 会产生 frame IRQ；5-step mode 不产生 frame IRQ。
- length counter、linear counter、envelope、sweep 要按 quarter/half frame 更新。
- DMC rate table、sample address、sample length、IRQ、loop 都要进入状态序列化。
- DMC DMA request 由 `Machine` 消耗 CPU bus cycle 并 supply byte。

桌面端不要在音频 callback 中推进模拟器。模拟器主循环产生 sample，SDL audio stream 只消费 sample。

## 11. DMA

OAM DMA：

- CPU 写 `$4014` 设置待执行 DMA page。
- 指令结束后执行 DMA。
- 先消耗 1 个 dummy cycle；如果起始 CPU cycle parity 需要对齐，再消耗 1 个 dummy cycle。
- 256 次 CPU read + 256 次 PPU OAM write。
- 总耗时 513 或 514 CPU cycles，连同写 `$4014` 的指令可在测试中观察。

DMC DMA：

- APU 产生 DMC DMA request。
- Machine 在安全点处理请求。
- 消耗 halt/alignment/dummy/read cycles。
- 读到的数据 supply 回 APU。

DMC/OAM DMA 更复杂的碰撞边界属于后续 hardening 项，但基础 DMC 测试已通过。

## 12. 调试器

调试器分为核心服务和桌面 UI。

核心 `Debugger` 提供：

- execute breakpoint。
- read/write watchpoint。
- instruction step。
- bounded trace ring。
- pause reason。
- bus access observation。

桌面 Dear ImGui UI 提供：

- CPU register/flags。
- disassembly。
- CPU memory view。
- trace view。
- breakpoint/watchpoint 面板。
- PPU/APU debug state。
- 指令、扫描线、帧级步进。

关键原则是调试器不能改变被调试程序行为。反汇编和内存查看必须使用 `peek`。

## 13. 存档、SRAM 与倒带

电池存档：

- Cartridge 暴露 battery size/data/load。
- 桌面端在加载 ROM 时尝试读取 `.sav`。
- 写入时使用临时文件 + rename，避免崩溃时破坏旧存档。

即时存档：

- `Machine::save_state()` 输出版本化 byte vector。
- header 包含 magic、version、ROM fingerprint、mapper id。
- CPU/PPU/APU/Bus/Cartridge/Mapper 逐字段写入。
- `load_state()` 使用事务式恢复：先备份当前状态，失败则回滚。

倒带：

- 桌面端按固定帧间隔保存 state snapshot。
- 按住 Backspace 时加载历史 snapshot。
- 首版采用有限数量快照，避免无限内存增长。

## 14. 桌面前端

桌面端只负责宿主能力：

- SDL3 window。
- SDL3 Renderer nearest-neighbor texture。
- 4:3 correction、integer square pixel option、fullscreen。
- Keyboard/gamepad input。
- SDL audio stream。
- ROM file dialog、drag/drop。
- Menu bar。
- Recent ROM list。
- Save/load slot。
- Rewind。
- ImGui debugger。

桌面端每帧的大致流程：

```text
poll SDL events
  -> translate keyboard/gamepad to controller buttons
  -> handle menu/dialog/drag-drop/hotkeys

if not paused:
  run Machine until one NES frame
  pop APU samples into audio stream
  maybe store rewind snapshot

upload PPU framebuffer to SDL texture
render game viewport
render ImGui debugger/menu
present
```

不要把 emulation correctness 放进前端。前端可以控制速度、显示和输入，但核心行为必须能被 headless runner 复现。

## 15. Headless 工具

`nesemu_headless` 用于快速验证 ROM：

```text
nesemu_headless <rom> [frames] [screenshot.bmp] [autoplay]
```

输出内容包括：

- ROM format、mapper、PRG/CHR size。
- frame count。
- CPU PC。
- deterministic frame hash。
- audio sample count、peak、RMS。

用途：

- 确认 ROM 不 crash、不 jam。
- 确认画面非空。
- 确认音频非零。
- 生成截图供人工检查。
- 在 CI 或本地批量 smoke test。

`nesemu_rom_test` 用于 Blargg 风格 `$6000` signature 测试。

`nesemu_nestest` 用于逐条比较 `nestest.log`。

`nesemu_frame_test` 用于运行固定帧数并比较最终 frame hash，适合 MMC3 pass screen 这类无法通过 `$6000` 自动报告的 ROM。

## 16. 测试策略

测试分四层：

1. 单元测试。

覆盖 Cartridge、Controller、CPU、PPU、APU、Mapper、Machine、Debugger、Bus、State。

2. Golden trace。

`nestest` 必须匹配 8991 条 CPU 状态和 cycle。

3. 公共测试 ROM。

包括 `ppu_vbl_nmi`、`apu_test`、`mmc3_irq_tests`。

4. 私有兼容性 smoke test。

商业 ROM 不进入仓库，只用 hash 记录。运行固定帧数，检查 jam、音频、截图和 frame hash。

本项目当前外部测试门槛：

```text
nestest_trace       8991/8991
ppu_vbl_nmi         10/10
apu_test            8/8
mmc3_irq_tests      4/4
```

配置外部测试：

```powershell
cmake --preset dev -DNESEMU_TEST_ROM_DIR=<nes-test-roms repo root>
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## 17. 工程化编码方式

推荐实现顺序：

1. CMake 工程、核心库、测试框架、空 Machine。
2. Cartridge parser 和 Mapper 0。
3. CPU bus、CPU official opcode、`nestest` runner。
4. CPU unofficial opcode 和 dummy access 修正。
5. PPU register、VBlank/NMI、基本 framebuffer。
6. Dot-level background pipeline。
7. Sprite evaluation、sprite zero、odd frame skip。
8. SDL video/input，跑通 NROM 游戏。
9. APU pulse/triangle/noise/DMC/frame counter。
10. Mappers 1/2/3/4/7，重点修 MMC3 IRQ。
11. Debugger core 和 ImGui 面板。
12. SRAM、save state、rewind。
13. Headless conformance、frame hash、CI、packaging。

编码约束：

- 所有公共核心类型放 `include/nesemu`。
- 每个核心组件拥有自己的状态和 `save_state/load_state`。
- 不允许 raw struct dump。
- 不允许前端影响核心确定性。
- `read` 有副作用，`peek` 无副作用。
- 新 Mapper 必须有单元测试和至少一个 ROM smoke 或 frame-hash 测试。
- 修改 CPU/PPU/APU/Mapper 时必须跑对应外部 ROM 测试。
- 不提交 ROM、`.sav`、`.state`、build、toolchain、cache、ZIP。

## 18. 关键文件导览

| 文件 | 作用 |
|---|---|
| `include/nesemu/machine.hpp` | 核心总控 API |
| `src/core/machine.cpp` | CPU/PPU/APU/DMA/IRQ 调度 |
| `src/core/cpu.cpp` | 2A03 CPU 指令和寻址模式 |
| `src/core/ppu.cpp` | 2C02 dot-level PPU |
| `src/core/apu.cpp` | APU channel、frame counter、DMC、mixer |
| `src/core/system_bus.cpp` | CPU address map、mirror、open bus |
| `src/core/cartridge.cpp` | iNES/NES 2.0 parser |
| `src/core/mapper004.cpp` | MMC3 banking 和 scanline IRQ |
| `src/core/debugger.cpp` | breakpoint、watchpoint、trace |
| `src/desktop/main.cpp` | SDL3/ImGui 桌面宿主 |
| `src/headless/main.cpp` | CLI smoke runner |
| `tests/nestest_runner.cpp` | CPU golden trace |
| `tests/rom_test_runner.cpp` | `$6000` test ROM runner |
| `tests/frame_test_runner.cpp` | frame hash runner |

## 19. 易错点清单

CPU：

- Page crossing dummy read 必须发生。
- RMW 指令必须 dummy write 原值再写新值。
- `JMP ($xxFF)` 必须模拟 6502 wraparound bug。
- IRQ/NMI 采样时机不能只在指令末尾粗略处理。
- Unofficial NOP 的 memory form 仍要读内存。

PPU：

- `$2002` 读取会清 VBlank 和 write toggle。
- `$2007` palette read 不走普通 read buffer 规则，但仍会更新 buffer。
- `$2006/$2007` 地址变化会影响 MMC3 A12。
- odd frame skip 只在 rendering enabled 时发生。
- Palette mirror `$3F10/$3F14/$3F18/$3F1C` 要映射到 universal background。

APU：

- `$4017` frame counter 写入有延迟。
- DMC rate table 数值必须准确。
- DMC DMA 会影响 CPU bus timing。
- `$4015` 读会清 frame IRQ。

Mapper：

- Mapper CPU read/write 和 PPU read/write 是两条独立路径。
- Mapper IRQ line 要被 CPU IRQ line 合并采样。
- MMC3 A12 filter 要抑制短脉冲。
- Mapper state 必须参与 save state。

工程：

- 不要把 SDL 类型泄露进 core。
- 不要用文件系统路径作为核心状态。
- 不要在音频 callback 推进模拟器。
- 不要把商业 ROM 放入 repo。
- 不要让调试器读操作改变 emulated state。

## 20. 复现验收标准

一个复现版本至少应达到：

```text
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure

cmake --preset desktop
cmake --build --preset desktop

cmake --preset desktop-release
cmake --build --preset desktop-release
cmake --build build/desktop-release --target package
```

功能验收：

- GUI 可以打开 ROM、显示画面、播放声音、响应键盘输入。
- `nestest` 8991/8991。
- `ppu_vbl_nmi` 10/10。
- `apu_test` 8/8。
- `mmc3_irq_tests` 4/4。
- NROM、CNROM、MMC3 至少各一个真实 ROM smoke 运行 1200 帧不 jam。
- Debugger 能断点、单步、查看内存和 trace。
- 快速存档/读档能 round-trip。
- 电池存档写入 ROM 同目录 `.sav`。
- Release ZIP 解压后可直接运行。

## 22. 后续扩展路线

优先级建议：

1. 更完整的 sprite overflow、OAM decay/open bus、DMC/OAM DMA collision。
2. PPU pattern table、nametable、palette、OAM 可视化面板。
3. 条件断点和表达式求值。
4. Mapper 9/10/11/66/71/206。
5. PAL/Dendy timing。
6. NSF 或扩展音频。
7. Linux/macOS 桌面 release packaging。
8. 更长时间音频 drift/underrun soak test。

复现时不要急着扩展。先把当前首版门槛全部跑绿，再开始加 Mapper 和平台。
