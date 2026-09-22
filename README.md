# nesemu-mimo

NTSC NES / Famicom emulator (C++20): CPU / PPU / APU core, Mapper 0/1/2/3/4/7,
SDL3 desktop with ImGui debugger, headless tools and regression tests.

## Features

- NTSC timing (~29780.5 CPU cycles / frame)
- iNES and NES 2.0 headers
- Mappers: NROM, MMC1, UxROM, CNROM, MMC3 (IRQ), AxROM
- 2A03 CPU (official + common unofficial opcodes)
- 2C02 PPU (dot-level), 2A03 APU (pulse / triangle / noise / DMC)
- Dual controllers, battery SRAM, save states, rewind
- Desktop (SDL3 + ImGui) and headless CLI

## Build

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure

cmake --preset desktop
cmake --build --preset desktop
```

External test ROMs (optional):

```powershell
cmake --preset dev -DNESEMU_TEST_ROM_DIR=<nes-test-roms root>
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## Run

```powershell
.\build\desktop\nesemu_desktop.exe "game.nes"
.\build\dev\nesemu_headless.exe game.nes 120 shot.bmp
```

## FPS overlay

- **UI xx** — window redraw rate
- **NES 60Hz** — emulation is paced at 60 frames/s (independent of display refresh)

## Docs

- [User guide](docs/USER_GUIDE.md)
- [Implementation report](docs/IMPLEMENTATION_REPORT.md)
- [Technical notes](docs/TECHNICAL_IMPLEMENTATION.md)

## Tests

- Unit tests (30)
- nestest 8991/8991
- ppu_vbl_nmi 10/10, apu_test 8/8, mmc3_irq_tests pass

## License

Personal / educational use. Commercial ROMs are not included.
