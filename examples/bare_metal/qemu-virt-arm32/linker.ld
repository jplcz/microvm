/* SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 128M
}

SECTIONS
{
    . = 0x40000000;

    .text : {
        *(.text.boot)
        *(.text .text.*)
    } > RAM

    .rodata : {
        *(.rodata .rodata.*)
    } > RAM

    .data : {
        *(.data .data.*)
    } > RAM

    .bss : {
        . = ALIGN(8);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
        . = ALIGN(8);
    } > RAM
}
