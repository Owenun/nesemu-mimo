# nesemu-mimo

NTSC NES / Famicom 模拟器（C++20）：CPU / PPU / APU 核心，Mapper 0/1/2/3/4/7，SDL3 桌面端（含 ImGui 调试器），以及无头测试工具与回归测试。

## 功能

- NTSC 时序（约 29780.5 CPU 周期 / 帧）
- 支持 iNES 与 NES 2.0 ROM 头
- Mapper：NROM、MMC1、UxROM、CNROM、MMC3（含 IRQ）、AxROM
- 2A03 CPU（官方指令 + 常用非官方指令）
- 2C02 PPU（点级）、APU（脉冲 / 三角 / 噪声 / DMC）
- 双手柄、电池 SRAM、即时存档、倒带
- 桌面端与命令行无头工具

## 构建

```powershell
# 核心 + 单元测试
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure

# 桌面端
cmake --preset desktop
cmake --build --preset desktop
```

外部测试 ROM（可选，需 [nes-test-roms](https://github.com/christopherpow/nes-test-roms)）：

```powershell
cmake --preset dev -DNESEMU_TEST_ROM_DIR=<nes-test-roms 路径>
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## 运行

```powershell
.\build\desktop\nesemu_desktop.exe "游戏.nes"
.\build\dev\nesemu_headless.exe 游戏.nes 120 shot.bmp
```

## 帧率说明

- **UI xx**：窗口每秒重绘次数（随显示器刷新变化，例如 60–144）
- **NES 60Hz**：模拟器按 60 游戏帧/秒推进，与 UI 帧率无关，保证原速

## 文档

- [用户手册](docs/USER_GUIDE.md)
- [实现报告](docs/IMPLEMENTATION_REPORT.md)
- [技术实现说明](docs/TECHNICAL_IMPLEMENTATION.md)

## 测试

- 单元测试 30 项
- nestest 8991/8991
- ppu_vbl_nmi 10/10、apu_test 8/8、mmc3_irq_tests 通过

## 许可

仅供个人 / 学习使用。仓库不包含商业 ROM。


## 文档

- [实现报告](docs/IMPLEMENTATION_REPORT.md)
- [使用手册](docs/USER_GUIDE.md)
- [测试报告](docs/TEST_REPORT.md)
- [缺陷报告](docs/BUG_REPORT.md)
- [技术说明](TECHNICAL_IMPLEMENTATION.md)
