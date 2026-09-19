// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

/** @file vm_code_gen_demo.cpp
 * @brief Demonstrates building `micro_vm` bytecode with `vm_code_generator`
 * and executing it against a register bank, local memory, and scratch RAM.
 */

#include <array>
#include <cstdint>
#include <cstring>

#include <microfmt/inspector/address_space.hpp>
#include <microvm/micro_vm.hpp>
#include <microfmt/inspector/register_context.hpp>
#include <microvm/vm_code_gen.hpp>
#include <microfmt/sinks/stdio.hpp>

using namespace microfmt::inspector;

namespace {

// A tiny fixed-size register bank exposed to the VM through
// `microfmt::register_context_ref`. Registers are `uint64_t` so their width
// matches the VM's load_register/store_register operand size.
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

int main() {
  auto out = microfmt::stdout_sink();
  microfmt::println(out, "=== micro_vm bytecode generator demo ===");

  // A single register bank, address space, and scratch buffer shared by all
  // the mini-programs below.
  register_bank regs{};
  std::byte reg_scratch[8]{};
  auto space = microfmt::address_space_ref::make<microfmt::local_space_tag>();
  auto reg_ctx = microfmt::make_register_context_ref<read_register, write_register>(regs, space, reg_scratch);

  // --------------------------------------------------------------------
  // Program 1: arithmetic expression, generated fluently and printed by
  // the VM itself via `print_int`.
  // --------------------------------------------------------------------
  {
    instruction code[16];
    vm_code_generator gen(code);
    gen.push(5).push(3).add().push(2).mul().print_int().halt();

    uint64_t stack[8]{};
    uint8_t work_ram[8]{};

    microfmt::print(out, "\n[1] (5 + 3) * 2 = ");
    auto result = memory_vm_executor::execute(gen.program(), space, reg_ctx, stack, work_ram, out);
    microfmt::println(out, "  (success={}, {} of {} instructions emitted)", result.success, gen.size(), 16);
  }

  // --------------------------------------------------------------------
  // Program 2: store a value into a register, load it back, and print it
  // as hexadecimal.
  // --------------------------------------------------------------------
  {
    instruction code[16];
    vm_code_generator gen(code);
    gen.push(0xdead).store_reg(2).load_reg(2).print_hex().halt();

    uint64_t stack[8]{};
    uint8_t work_ram[8]{};

    microfmt::print(out, "[2] store r2=0xdead, load r2, print_hex -> ");
    auto result = memory_vm_executor::execute(gen.program(), space, reg_ctx, stack, work_ram, out);
    microfmt::println(out, "  (success={}, regs[2]={:#x})", result.success, regs[2]);
  }

  // --------------------------------------------------------------------
  // Program 3: fault-safe target memory round trip through `read_u32`/
  // `store_u32` against an ordinary local variable.
  // --------------------------------------------------------------------
  {
    uint32_t target = 0;
    const auto addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&target));

    instruction code[16];
    vm_code_generator gen(code);
    gen.push(addr).push(0xcafef00d).store_u32().push(addr).read_u32().print_hex().halt();

    uint64_t stack[8]{};
    uint8_t work_ram[8]{};

    microfmt::print(out, "[3] store_u32/read_u32 round trip -> ");
    auto result = memory_vm_executor::execute(gen.program(), space, reg_ctx, stack, work_ram, out);
    microfmt::println(out, "  (success={}, target={:#x})", result.success, target);
  }

  // --------------------------------------------------------------------
  // Program 4: conditional branching computing max(a, b) using
  // `current_offset()` to record forward-jump targets as the program is
  // assembled.
  // --------------------------------------------------------------------
  {
    constexpr uint64_t a = 7;
    constexpr uint64_t b = 12;

    instruction code[16];
    vm_code_generator gen(code);
    gen.push(a).push(b).less_than(); // stack: (a < b) ? 1 : 0

    // Reserve the branch now; its target is only known once the "use a"
    // block below has been emitted, so the offset is recorded up front.
    const size_t use_a_offset = 6; // push(a) below lands at instruction index 6
    const size_t end_offset = 7;   // print_int lands at instruction index 7

    gen.branch_zero(use_a_offset) // a >= b: skip straight to the "use a" block
        .push(b)                 // a < b: max is b
        .jump(end_offset)
        .push(a) // use_a_offset: max is a
        .print_int()
        .halt();

    uint64_t stack[8]{};
    uint8_t work_ram[8]{};

    microfmt::print(out, "[4] max(7, 12) = ");
    auto result = memory_vm_executor::execute(gen.program(), space, reg_ctx, stack, work_ram, out);
    microfmt::println(out, "  (success={})", result.success);
  }

  // --------------------------------------------------------------------
  // Program 5: `work_ram_print` streaming a caller-populated scratch
  // buffer straight to the output sink.
  // --------------------------------------------------------------------
  {
    uint8_t work_ram[16]{};
    const char message[] = "VM says hi!";
    std::memcpy(work_ram, message, sizeof(message) - 1);

    instruction code[8];
    vm_code_generator gen(code);
    gen.push(0).push(sizeof(message) - 1).work_ram_print().halt();

    uint64_t stack[8]{};

    microfmt::print(out, "[5] work_ram_print -> \"");
    auto result = memory_vm_executor::execute(gen.program(), space, reg_ctx, stack, work_ram, out);
    microfmt::println(out, "\"  (success={})", result.success);
  }

  return 0;
}
