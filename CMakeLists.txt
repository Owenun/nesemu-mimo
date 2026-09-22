cmake_minimum_required(VERSION 3.28)
project(nesemu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(NESEMU_BUILD_DESKTOP "Build SDL3 desktop frontend" ON)
option(NESEMU_BUILD_TESTS "Build unit tests and ROM runners" ON)
option(NESEMU_BUILD_FUZZ "Build libFuzzer cartridge fuzzer" OFF)
option(NESEMU_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(NESEMU_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(NESEMU_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

set(NESEMU_TEST_ROM_DIR "" CACHE PATH "Path to nes-test-roms repository root")

if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

function(nesemu_strict_warnings target)
  if(MSVC)
    if(NESEMU_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    if(NESEMU_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

function(nesemu_allow_third_party_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W3 /WX-)
  else()
    target_compile_options(${target} PRIVATE -Wno-error)
  endif()
endfunction()

if(NESEMU_ENABLE_ASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()
if(NESEMU_ENABLE_UBSAN)
  add_compile_options(-fsanitize=undefined)
  add_link_options(-fsanitize=undefined)
endif()

add_library(nesemu_core STATIC
  src/core/cartridge.cpp
  src/core/mapper.cpp
  src/core/mapper000.cpp
  src/core/mapper001.cpp
  src/core/mapper002.cpp
  src/core/mapper003.cpp
  src/core/mapper004.cpp
  src/core/mapper007.cpp
  src/core/cpu.cpp
  src/core/ppu.cpp
  src/core/apu.cpp
  src/core/system_bus.cpp
  src/core/machine.cpp
  src/core/debugger.cpp
  src/core/state.cpp
  src/core/controller.cpp
)

target_include_directories(nesemu_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(nesemu_core PUBLIC cxx_std_20)
nesemu_strict_warnings(nesemu_core)

add_executable(nesemu_headless src/headless/main.cpp)
target_link_libraries(nesemu_headless PRIVATE nesemu_core)
if(WIN32)
  target_link_libraries(nesemu_headless PRIVATE shell32)
endif()
nesemu_strict_warnings(nesemu_headless)

add_executable(nesemu_debug src/headless/debug_dump.cpp)
target_link_libraries(nesemu_debug PRIVATE nesemu_core)
nesemu_strict_warnings(nesemu_debug)

add_executable(nesemu_sim src/headless/sim_scroll.cpp)
target_link_libraries(nesemu_sim PRIVATE nesemu_core)
if(WIN32)
  target_link_libraries(nesemu_sim PRIVATE shell32)
endif()
nesemu_strict_warnings(nesemu_sim)

add_executable(nesemu_trace src/headless/trace_pc.cpp)
target_link_libraries(nesemu_trace PRIVATE nesemu_core)
nesemu_strict_warnings(nesemu_trace)

if(NESEMU_BUILD_TESTS)
  enable_testing()

  add_executable(nesemu_tests
    tests/test_main.cpp
    tests/test_cartridge.cpp
    tests/test_cpu.cpp
    tests/test_ppu.cpp
    tests/test_ppu_render.cpp
    tests/test_apu.cpp
    tests/test_mapper.cpp
    tests/test_mmc3_irq.cpp
    tests/test_state.cpp
  )
  target_link_libraries(nesemu_tests PRIVATE nesemu_core)
  nesemu_strict_warnings(nesemu_tests)
  add_test(NAME unit_tests COMMAND nesemu_tests)

  add_executable(nesemu_nestest tests/nestest_runner.cpp)
  target_link_libraries(nesemu_nestest PRIVATE nesemu_core)
  nesemu_strict_warnings(nesemu_nestest)

  add_executable(nesemu_rom_test tests/rom_test_runner.cpp)
  target_link_libraries(nesemu_rom_test PRIVATE nesemu_core)
  nesemu_strict_warnings(nesemu_rom_test)

  add_executable(nesemu_frame_test tests/frame_test_runner.cpp)
  target_link_libraries(nesemu_frame_test PRIVATE nesemu_core)
  nesemu_strict_warnings(nesemu_frame_test)

  if(NESEMU_TEST_ROM_DIR)
    set(NESEMU_TEST_ROMS ${NESEMU_TEST_ROM_DIR})
    if(EXISTS "${NESEMU_TEST_ROMS}/other/nestest.nes")
      add_test(NAME nestest_trace
        COMMAND nesemu_nestest
        "${NESEMU_TEST_ROMS}/other/nestest.nes"
        "${NESEMU_TEST_ROMS}/other/nestest.log")
    endif()
    # ppu_vbl_nmi: 10 individual ROMs
    set(PPU_VBL_COUNT 0)
    foreach(n RANGE 1 10)
      if(n LESS 10)
        set(pad "0${n}")
      else()
        set(pad "${n}")
      endif()
      set(rom "${NESEMU_TEST_ROMS}/ppu_vbl_nmi/rom_singles/${pad}-vbl_basics.nes")
      # names are 01-vbl_basics, 02-vbl_set_time, ...
    endforeach()
    file(GLOB PPU_VBL_ROMS "${NESEMU_TEST_ROMS}/ppu_vbl_nmi/rom_singles/*.nes")
    list(LENGTH PPU_VBL_ROMS PPU_VBL_N)
    if(PPU_VBL_N GREATER 0)
      add_test(NAME ppu_vbl_nmi
        COMMAND nesemu_rom_test "${NESEMU_TEST_ROMS}/ppu_vbl_nmi/ppu_vbl_nmi.nes" 600)
    endif()
    if(EXISTS "${NESEMU_TEST_ROMS}/apu_test/rom_singles/1-len_ctr.nes")
      add_test(NAME apu_test
        COMMAND nesemu_rom_test
        "${NESEMU_TEST_ROMS}/apu_test/rom_singles/1-len_ctr.nes" 300)
    endif()
    # Doc threshold: mmc3_irq_tests 4/4 — primary suite is 1.Clocking (codes 2-9).
    # Additional ROMs 3.A12_clocking and 6.MMC3_rev_B also expected to pass.
    # 5.MMC3_rev_A is mutually exclusive with 6 (rev A vs B).
    if(EXISTS "${NESEMU_TEST_ROMS}/mmc3_irq_tests/1.Clocking.nes")
      add_test(NAME mmc3_irq_tests
        COMMAND nesemu_rom_test
        "${NESEMU_TEST_ROMS}/mmc3_irq_tests/1.Clocking.nes" 300)
    endif()
    # Combined multi-ROM runners
    if(EXISTS "${NESEMU_TEST_ROMS}/ppu_vbl_nmi/rom_singles/01-vbl_basics.nes")
      add_test(NAME ppu_vbl_nmi_01 COMMAND nesemu_rom_test
        "${NESEMU_TEST_ROMS}/ppu_vbl_nmi/rom_singles/01-vbl_basics.nes" 300)
    endif()
    if(EXISTS "${NESEMU_TEST_ROMS}/apu_test/rom_singles/1-len_ctr.nes")
      # also run remaining apu singles
      foreach(a 2-len_table 3-irq_flag 4-jitter 5-len_timing 6-irq_flag_timing 7-dmc_basics 8-dmc_rates)
        if(EXISTS "${NESEMU_TEST_ROMS}/apu_test/rom_singles/${a}.nes")
          add_test(NAME apu_${a} COMMAND nesemu_rom_test
            "${NESEMU_TEST_ROMS}/apu_test/rom_singles/${a}.nes" 300)
        endif()
      endforeach()
    endif()
    file(GLOB MMC3_IRQ_ROMS
      "${NESEMU_TEST_ROMS}/mmc3_irq_tests/*.nes"
      "${NESEMU_TEST_ROMS}/mmc3_irq_tests/rom_singles/*.nes")
    foreach(r ${MMC3_IRQ_ROMS})
      get_filename_component(rname ${r} NAME_WE)
      add_test(NAME mmc3_irq_${rname} COMMAND nesemu_rom_test ${r} 300)
    endforeach()
    file(GLOB PPU_VBL_SINGLE "${NESEMU_TEST_ROMS}/ppu_vbl_nmi/rom_singles/*.nes")
    foreach(r ${PPU_VBL_SINGLE})
      get_filename_component(rname ${r} NAME_WE)
      add_test(NAME ppu_vbl_${rname} COMMAND nesemu_rom_test ${r} 300)
    endforeach()
  endif()
endif()

if(NESEMU_BUILD_DESKTOP)
  include(FetchContent)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.0
    GIT_SHALLOW TRUE
  )
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL3)
  nesemu_allow_third_party_warnings(SDL3-static)
  if(TARGET SDL3-shared)
    nesemu_allow_third_party_warnings(SDL3-shared)
  endif()
  if(TARGET SDL3_uclibc)
    nesemu_allow_third_party_warnings(SDL3_uclibc)
  endif()
  if(TARGET SDL3_headers)
    nesemu_allow_third_party_warnings(SDL3_headers)
  endif()

  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(imgui)

  add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_include_directories(imgui_backend PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  target_link_libraries(imgui_backend PUBLIC SDL3::SDL3)

  add_executable(nesemu_desktop
    src/desktop/main.cpp
    src/desktop/audio.cpp
    src/desktop/palette.cpp
  )
  target_link_libraries(nesemu_desktop PRIVATE nesemu_core imgui_backend SDL3::SDL3)
  if(WIN32)
    target_link_libraries(nesemu_desktop PRIVATE shell32)
  endif()
  nesemu_strict_warnings(nesemu_desktop)
  nesemu_allow_third_party_warnings(imgui_backend)
endif()

if(NESEMU_BUILD_FUZZ)
  add_executable(nesemu_fuzz tests/fuzz/fuzz_cartridge.cpp)
  target_link_libraries(nesemu_fuzz PRIVATE nesemu_core)
  target_compile_options(nesemu_fuzz PRIVATE -fsanitize=fuzzer)
  target_link_options(nesemu_fuzz PRIVATE -fsanitize=fuzzer)
endif()

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "nesemu")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "nesemu-${PROJECT_VERSION}-windows-x64")
if(NESEMU_BUILD_DESKTOP)
  install(TARGETS nesemu_desktop nesemu_headless RUNTIME DESTINATION .)
else()
  install(TARGETS nesemu_headless RUNTIME DESTINATION .)
endif()
include(CPack)
