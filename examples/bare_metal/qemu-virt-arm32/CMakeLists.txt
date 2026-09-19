# SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
#
# SPDX-License-Identifier: BSD-2-Clause

# Standalone freestanding CMake project - configure with the bundled
# toolchain file, e.g.:
#
#   cmake -S . -B build -G Ninja \
#       -DCMAKE_TOOLCHAIN_FILE=toolchain-arm32.cmake
#   cmake --build build
#   cmake --build build --target run
#
# This is intentionally *not* added as a subdirectory of the main
# jplcz_microvm build: it cross-compiles freestanding code for a different
# target triple/ABI than the host build and requires its own toolchain file.
cmake_minimum_required(VERSION 3.20)
project(jplcz_microvm_qemu_virt_arm32 ASM C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    message(WARNING
        "This project targets bare-metal 32-bit ARM. Configure it with "
        "-DCMAKE_TOOLCHAIN_FILE=toolchain-arm32.cmake, or pass "
        "-DJPLCZ_ARM32_CROSS_PREFIX=... to select a different cross "
        "toolchain prefix.")
endif()

set(JPLCZ_MICROVM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
    CACHE PATH "Path to the jplcz_microvm include/ directory")
set(JPLCZ_MICROFMT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../microfmt/include"
    CACHE PATH "Path to a jplcz_microfmt include/ directory (sibling checkout by default)")

add_executable(firmware.elf start.S main.cpp libc_stubs.c aeabi_uldivmod.S udivmoddi4.c)

target_include_directories(firmware.elf PRIVATE
    "${JPLCZ_MICROVM_INCLUDE_DIR}"
    "${JPLCZ_MICROFMT_INCLUDE_DIR}")

target_compile_options(firmware.elf PRIVATE
    -mcpu=cortex-a15
    -marm
    -mno-unaligned-access
    -fno-exceptions
    -fno-rtti
    -fno-pic
    -fno-stack-protector
    -Os
    -Wall
    -Wextra
    -Wno-psabi
    -DMICROFMT_DISABLE_ASSERT_STDIO=1)

set_source_files_properties(libc_stubs.c PROPERTIES
    COMPILE_OPTIONS "-fno-builtin")

target_link_options(firmware.elf PRIVATE
    -mcpu=cortex-a15
    -marm
    -nostdlib
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/firmware.map)

set_target_properties(firmware.elf PROPERTIES
    SUFFIX ""
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")

find_program(JPLCZ_QEMU_SYSTEM_ARM qemu-system-arm)
if(JPLCZ_QEMU_SYSTEM_ARM)
    add_custom_target(run
        COMMAND ${JPLCZ_QEMU_SYSTEM_ARM} -M virt -cpu cortex-a15 -nographic -kernel $<TARGET_FILE:firmware.elf>
        DEPENDS firmware.elf
        USES_TERMINAL)
endif()
