// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

/** @file main.cpp
 * @brief Minimal 32-bit ARM (ARMv7-A) freestanding kernel for QEMU's `virt`
 * machine. Boots on a single core, brings up the PL011 UART via
 * `microfmt::pl011_sink`, and runs a few small `jplcz_microvm` programs -
 * both hand-generated with `vm_code_generator` and compiled from a tiny
 * script with `vm_compiler` - against real RAM, a scratch register bank,
 * and scratch work RAM, streaming their output straight to the UART.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <microfmt/hw/pl011_sink.hpp>
#include <microfmt/inspector/address_space.hpp>
#include <microfmt/inspector/gdb_registers.hpp>
#include <microfmt/inspector/register_context.hpp>
#include <microfmt/microfmt.hpp>
#include <microvm/micro_vm.hpp>
#include <microvm/vm_code_gen.hpp>
#include <microvm/vm_compiler.hpp>

// Freestanding libc shims (strlen/memchr/memcpy/memset) required when
// building with -nostdlib live in libc_stubs.c, compiled as plain C so
// their C-ABI definitions never collide with libstdc++'s <cstring>
// overload/asm-redirection declarations for these names.

using namespace microfmt::inspector;

namespace {

// ----------------------------------------------------------------------
// A tiny fixed-size register bank exposed to the VM through
// `microfmt::register_context_ref`. Registers are `uint64_t` so their
// width matches the VM's load_reg/store_reg operand size.
// ----------------------------------------------------------------------

using register_bank = std::array<uint64_t, 4>;

bool read_register(const register_bank *state, microfmt::address_space_ref, uint32_t index, void *dest,
                   size_t size) noexcept {
  if (!state || size != sizeof(uint64_t) || index >= state->size())
    return false;
  std::memcpy(dest, &(*state)[index], sizeof(uint64_t));
  return true;
}

bool write_register(register_bank *state, microfmt::address_space_ref, uint32_t index, const void *src,
                    size_t size) noexcept {
  if (!state || size != sizeof(uint64_t) || index >= state->size())
    return false;
  std::memcpy(&(*state)[index], src, sizeof(uint64_t));
  return true;
}

} // namespace

// ----------------------------------------------------------------------
// microfmt library assertion handler. Registered before anything else
// runs so that any MICROFMT_ASSERT failure (e.g. an out-of-bounds span
// access) reports over the UART instead of silently falling through to
// MICROFMT_TRAP()'s __builtin_trap() (still caught by vector_undef, but
// without the "what/where" context this gives us).
// ----------------------------------------------------------------------

extern "C" void bare_metal_assert_handler(const char *expr, const char *file, int line, const char *msg) {
  microfmt::pl011_sink uart(0x09000000, /*translate_crlf=*/true);
  const auto sink = uart.as_sink();

  microfmt::format_to(sink, "\n!!! MICROFMT ASSERT FAILED !!!\n");
  microfmt::format_to(sink, "  expr: {}\n", microfmt::string_view(expr));
  microfmt::format_to(sink, "  at:   {}:{}\n", microfmt::string_view(file), line);
  microfmt::format_to(sink, "  msg:  {}\n", microfmt::string_view(msg));
}

// ----------------------------------------------------------------------
// CPU exception handler, entered from the vector table in start.S with
// r0=exception index, r1=faulting instruction address. Re-creates a fresh
// UART sink (cheap - it is just a base address + a flag) since we cannot
// rely on any state kernel_main happened to have on its stack, then
// prints a short diagnostic and halts.
// ----------------------------------------------------------------------

extern "C" void exception_handler(uint32_t code, uint32_t addr) {
  static constexpr microfmt::string_view names[] = {
      "undefined instruction", "software interrupt", "prefetch abort", "data abort", "irq", "fiq",
  };

  microfmt::pl011_sink uart(0x09000000, /*translate_crlf=*/true);
  const auto sink = uart.as_sink();

  microfmt::string_view name =
      code < (sizeof(names) / sizeof(names[0])) ? names[code] : microfmt::string_view("unknown");
  microfmt::format_to(sink, "\n!!! CPU EXCEPTION: {} at pc={:#010x} !!!\n", name, addr);

  while (true) {
    __asm__ volatile("wfi");
  }
}

extern "C" void kernel_main() {
  microfmt::pl011_sink uart(0x09000000, /*translate_crlf=*/true);
  uart.enable();
  const auto sink = uart.as_sink();

  microfmt::set_assert_handler(bare_metal_assert_handler);

  microfmt::format_to(sink, "\n==================================================\n");
  microfmt::format_to(sink, " [KERNEL] 32-bit ARM (ARMv7-A) microvm demo\n");
  microfmt::format_to(sink, "==================================================\n");

  register_bank regs{};
  std::byte reg_scratch[8]{};
  auto space = microfmt::address_space_ref::make<microfmt::local_space_tag>();
  auto reg_ctx = microfmt::make_register_context_ref<read_register, write_register>(regs, space, reg_scratch);

  uint64_t stack[16]{};
  uint8_t work_ram[32]{};

  // --------------------------------------------------------------------
  // Program 1: arithmetic + a counted loop, purely on the VM's own
  // evaluation stack - no target memory or registers touched.
  // --------------------------------------------------------------------
  {
    instruction code[32];
    vm_code_generator gen(code);

    // r0 = 5, r1 = 3 -> print (r0 + r1) * 2
    gen.push(5).push(3).add().push(2).mul().store_reg(0).load_reg(0).print_hex();
    gen.push('\n').print_char();

    // for (i = 0; i < 4; ++i) print_int(i);
    gen.push(0).store_reg(1);
    size_t loop_top = gen.size();
    gen.load_reg(1).push(4).less_than();
    size_t branch_slot = gen.size();
    gen.branch_zero(0); // patched below once the loop end address is known
    gen.load_reg(1).print_int();
    gen.push(' ').print_char();
    gen.load_reg(1).push(1).add().store_reg(1);
    gen.jump(static_cast<uint64_t>(loop_top));
    size_t loop_end = gen.size();
    gen.halt();
    code[branch_slot].operand = static_cast<uint64_t>(loop_end);

    microfmt::format_to(sink, "\n[1] (5 + 3) * 2 = ");
    auto result = memory_vm_executor::execute(gen.program(), space, reg_ctx, stack, work_ram, sink);
    microfmt::format_to(sink, "\n    loop 0..3     = ");
    microfmt::format_to(sink, "(success={}, {} instructions)\n", result.success, gen.size());
  }

  // --------------------------------------------------------------------
  // Program 2: fault-safe read of live target RAM - the first word of
  // this very kernel image, i.e. the ARM boot branch instruction at the
  // load address 0x40000000.
  // --------------------------------------------------------------------
  {
    instruction code[8];
    vm_code_generator gen(code);
    gen.push(0x40000000).read_u32().print_hex().halt();

    microfmt::format_to(sink, "\n[2] First word @ 0x40000000 = ");
    auto result = memory_vm_executor::execute(gen.program(), space, reg_ctx, stack, work_ram, sink);
    microfmt::format_to(sink, " (success={})\n", result.success);
  }

  // --------------------------------------------------------------------
  // Program 3: stash a short message into scratch work RAM, then have
  // the VM stream it back out through the sink with work_ram_print.
  // --------------------------------------------------------------------
  {
    constexpr char message[] = "hello from microvm";
    std::memcpy(work_ram, message, sizeof(message) - 1);

    instruction code[8];
    vm_code_generator gen(code);
    gen.push(0).push(sizeof(message) - 1).work_ram_print().halt();

    microfmt::format_to(sink, "\n[3] work_ram_print: ");
    auto result = memory_vm_executor::execute(gen.program(), space, reg_ctx, stack, work_ram, sink);
    microfmt::format_to(sink, " (success={})\n", result.success);
  }

  // --------------------------------------------------------------------
  // Program 4: compile a tiny C-like script with `vm_compiler` - built on
  // `vm_lexer` + `vm_code_generator` - backed entirely by static,
  // fixed-capacity tables for its instruction buffer, variable symbol
  // table, and label/patch tables. No heap allocation anywhere.
  // --------------------------------------------------------------------
  {
    constexpr microfmt::string_view script = R"(
      let a = 6;
      let b = 7;
      let product = a * b;
      print_int(product);
      print_char(32);
      print_hex(product);
      halt;
      )";

    instruction compiler_code[32];
    vm_code_generator compiler_gen(compiler_code);

    variable_symbol compiler_symbols[8]{};
    variable_context compiler_vars(compiler_symbols);

    label_allocator::label_info label_infos[8]{};
    label_allocator::patch_site patch_sites[8]{};
    label_allocator compiler_labels(label_infos, patch_sites);

    const auto arch = target_arch_traits::create<microfmt::gdb::tags::arm32>();

    microfmt::format_to(sink, "\n[4] vm_compiler (static tables): 6 * 7 = ");
    auto compile_result = vm_compiler::compile(script, compiler_gen, compiler_vars, arch, compiler_labels, sink);
    if (!compile_result.success) {
      microfmt::format_to(sink, "compilation failed\n");
    } else {
      auto result = memory_vm_executor::execute(compiler_gen.program(), space, reg_ctx, stack, work_ram, sink);
      microfmt::format_to(sink, " (success={})\n", result.success);
    }
  }

  microfmt::format_to(sink, "\nSystem entering low-power idle state (WFI)...\n");

  while (true) {
    __asm__ volatile("wfi");
  }
}
