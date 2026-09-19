@ SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
@
@ SPDX-License-Identifier: BSD-2-Clause

.arch armv7-a
.global _start
.section .text.boot

_start:
    @ Only allow the primary core (CPU 0) to boot; spin others in WFI.
    mrc p15, 0, r0, c0, c0, 5   @ Read MPIDR into r0
    and r0, r0, #0xFF
    cmp r0, #0
    bne 3f

    @ Set up the stack pointer in high RAM (QEMU virt RAM starts at 0x40000000).
    ldr sp, =0x48000000

    @ Zero out the BSS section.
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:  cmp r0, r1
    bge 2f
    str r2, [r0], #4
    b 1b

2:
    @ Jump to the C++ entry point.
    bl kernel_main

3:
    @ Hang secondary cores in low-power state.
    wfi
    b 3b
