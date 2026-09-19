<!--
SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>

SPDX-License-Identifier: BSD-2-Clause
-->

# 32-bit ARM (ARMv7-A) Bare-Metal `jplcz_microvm` Demo

A minimal, freestanding ARMv7-A kernel for QEMU's `virt` machine that
executes small `jplcz_microvm` bytecode programs and streams their output
straight to the ARM PL011 UART via `microfmt::pl011_sink` - no OS, no libc,
no dynamic allocation.

It demonstrates:

- Booting a single CPU core on QEMU `virt`, zeroing `.bss`, and setting up
  per-mode stacks.
- Installing an ARMv7-A exception vector table via `VBAR` so undefined
  instructions, aborts, and IRQ/FIQ report a diagnostic over the UART
  instead of hanging silently.
- Registering a `microfmt` assertion handler that also reports over the
  UART, so library-level `MICROFMT_ASSERT` failures are visible.
- Running `jplcz_microvm` bytecode (built with `vm_code_generator`) against
  a scratch register bank, live target RAM (`address_space_ref`), and
  scratch work RAM, printing results with `print_int`/`print_hex`/
  `work_ram_print`.

## Layout

| File                    | Purpose                                                              |
|-------------------------|-----------------------------------------------------------------------|
| `start.S`                | Reset entry point: single-core gating, per-mode stack setup, `VBAR` install, `.bss` zeroing, exception vector table. |
| `main.cpp`               | `kernel_main`, the assert/exception handlers, and the three demo `microvm` programs. |
| `linker.ld`              | Maps the image into QEMU `virt` RAM starting at `0x40000000`.          |
| `libc_stubs.c`           | Freestanding `strlen`/`memchr`/`memcpy`/`memset`, compiled as plain C so they never collide with libstdc++'s `<cstring>` overload declarations. |
| `aeabi_uldivmod.S`, `udivmoddi4.c` | 64-bit division helpers the compiler emits calls to (`__aeabi_uldivmod`), ported from LLVM's compiler-rt since `-nostdlib` provides no libgcc/compiler-rt. |
| `toolchain-arm32.cmake`  | CMake toolchain file selecting the ARM cross compiler.                |

## Building and Running

Requires an ARM cross toolchain (`arm-linux-gnueabi-gcc/g++/as`, or override
via `-DJPLCZ_ARM32_CROSS_PREFIX=...`) and `qemu-system-arm`.

```bash
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchain-arm32.cmake
cmake --build build
cmake --build build --target run
```

`JPLCZ_MICROVM_INCLUDE_DIR` and `JPLCZ_MICROFMT_INCLUDE_DIR` cache variables
control where the two libraries' headers are found; by default they resolve
to `../../../include` (this repo) and a sibling `../../../../microfmt/include`
checkout.

### Expected output

```
==================================================
 [KERNEL] 32-bit ARM (ARMv7-A) microvm demo
==================================================

[1] (5 + 3) * 2 = 0x10
0 1 2 3
    loop 0..3     = (success=true, 26 instructions)

[2] First word @ 0x40000000 = 0xee100fb0 (success=true)

[3] work_ram_print: hello from microvm (success=true)

System entering low-power idle state (WFI)...
```

If a `jplcz_microvm` program or the surrounding boot code ever crashes, the
vector table and assert handler report it instead of silently hanging, e.g.:

```
!!! CPU EXCEPTION: data abort at pc=0x40000994 !!!
```

or

```
!!! MICROFMT ASSERT FAILED !!!
  expr: idx < size_
  at:   .../span.hpp:42
  msg:  span index out of bounds
```
