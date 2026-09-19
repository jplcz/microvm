# SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
#
# SPDX-License-Identifier: BSD-2-Clause

# CMake toolchain file for a freestanding, 32-bit ARMv7-A build targeting
# QEMU's `virt` machine. Use with:
#
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-arm32.cmake -G Ninja
#   cmake --build build
#
# Override JPLCZ_ARM32_CROSS_PREFIX to point at a different bare-metal or
# Linux ARM cross toolchain (e.g. arm-none-eabi-).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT DEFINED JPLCZ_ARM32_CROSS_PREFIX)
    set(JPLCZ_ARM32_CROSS_PREFIX "arm-linux-gnueabi-")
endif()

set(CMAKE_C_COMPILER "${JPLCZ_ARM32_CROSS_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${JPLCZ_ARM32_CROSS_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${JPLCZ_ARM32_CROSS_PREFIX}gcc")
set(CMAKE_OBJCOPY "${JPLCZ_ARM32_CROSS_PREFIX}objcopy" CACHE FILEPATH "")
set(CMAKE_OBJDUMP "${JPLCZ_ARM32_CROSS_PREFIX}objdump" CACHE FILEPATH "")

# There is no OS/libc to link against - skip compiler sanity checks that try
# to build+run a full executable, and never search the (nonexistent) target
# sysroot for host programs.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
