@ SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
@
@ SPDX-License-Identifier: BSD-2-Clause

.arch armv7-a

.equ MODE_MASK, 0x1F
.equ MODE_FIQ,  0x11
.equ MODE_IRQ,  0x12
.equ MODE_SVC,  0x13
.equ MODE_ABT,  0x17
.equ MODE_UND,  0x1B
.equ I_BIT,     0x80
.equ F_BIT,     0x40

.global _start
.section .text.boot

_start:
    @ Only allow the primary core (CPU 0) to boot; spin others in WFI.
    mrc p15, 0, r0, c0, c0, 5   @ Read MPIDR into r0
    and r0, r0, #0xFF
    cmp r0, #0
    bne 4f

    @ Set up a distinct stack for every exception mode we install a handler
    @ for, so undefined-instruction/abort/irq/fiq handlers can safely call
    @ into C++ without corrupting whatever the interrupted mode was doing.
    mrs r3, cpsr
    bic r3, r3, #MODE_MASK

    orr r4, r3, #(MODE_UND | I_BIT | F_BIT)
    msr cpsr_c, r4
    ldr sp, =und_stack_top

    orr r4, r3, #(MODE_ABT | I_BIT | F_BIT)
    msr cpsr_c, r4
    ldr sp, =abt_stack_top

    orr r4, r3, #(MODE_IRQ | I_BIT | F_BIT)
    msr cpsr_c, r4
    ldr sp, =irq_stack_top

    orr r4, r3, #(MODE_FIQ | I_BIT | F_BIT)
    msr cpsr_c, r4
    ldr sp, =fiq_stack_top

    orr r4, r3, #(MODE_SVC | I_BIT | F_BIT)
    msr cpsr_c, r4
    ldr sp, =0x48000000

    @ Point VBAR at our vector table so undefined-instruction/abort faults
    @ land in vector_table's handlers instead of free-running at address 0.
    ldr r0, =vector_table
    mcr p15, 0, r0, c12, c0, 0

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
    b 3b

4:
    @ Hang secondary cores in low-power state.
    wfi
    b 4b

@ ----------------------------------------------------------------------
@ Exception vector table. Must be 32-byte aligned; installed via VBAR
@ above. Each slot is a single branch so the whole table fits in the
@ architecturally-mandated 8 words.
@ ----------------------------------------------------------------------
.align 5
vector_table:
    b   3b                    @ 0x00 Reset (unused: QEMU jumps to _start directly)
    b   vector_undef          @ 0x04 Undefined Instruction
    b   vector_swi            @ 0x08 Supervisor Call (SWI/SVC)
    b   vector_prefetch_abort @ 0x0C Prefetch Abort
    b   vector_data_abort     @ 0x10 Data Abort
    b   3b                    @ 0x14 Reserved
    b   vector_irq            @ 0x18 IRQ
    b   vector_fiq            @ 0x1C FIQ

vector_undef:
    sub lr, lr, #4
    mov r0, #0
    mov r1, lr
    bl exception_handler
    b 3b

vector_swi:
    mov r0, #1
    mov r1, lr
    bl exception_handler
    b 3b

vector_prefetch_abort:
    sub lr, lr, #4
    mov r0, #2
    mov r1, lr
    bl exception_handler
    b 3b

vector_data_abort:
    sub lr, lr, #8
    mov r0, #3
    mov r1, lr
    bl exception_handler
    b 3b

vector_irq:
    mov r0, #4
    mov r1, lr
    bl exception_handler
    b 3b

vector_fiq:
    mov r0, #5
    mov r1, lr
    bl exception_handler
    b 3b

@ ----------------------------------------------------------------------
@ Small per-mode exception stacks.
@ ----------------------------------------------------------------------
.section .bss
.align 4
und_stack: .skip 1024
und_stack_top:
abt_stack: .skip 1024
abt_stack_top:
irq_stack: .skip 1024
irq_stack_top:
fiq_stack: .skip 1024
fiq_stack_top:
