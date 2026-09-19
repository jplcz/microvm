// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

/** @file vm_compiler_demo.cpp
 * @brief Compiles a small, static script with `vm_compiler` (built on top of
 * `vm_lexer` and `vm_code_generator`), dumps the resulting `micro_vm`
 * opcodes, then executes the program.
 */

#include <array>
#include <cstdint>
#include <cstring>

#include <microfmt/inspector/address_space.hpp>
#include <microfmt/inspector/fallible_address_space.hpp>
#include <microfmt/inspector/gdb_registers.hpp>
#include <microvm/micro_vm.hpp>
#include <microfmt/inspector/register_context.hpp>
#include <microvm/variable_context.hpp>
#include <microvm/vm_code_gen.hpp>
#include <microvm/vm_compiler.hpp>
#include <microfmt/sinks/stdio.hpp>

using namespace microfmt::inspector;

namespace {

// The script compiled and executed by this demo. `let` declares a
// work_ram-backed variable; `rax` is a real x86_64 register resolved through
// `target_arch_traits`; `print_int` prints the top-of-stack value.
constexpr microfmt::string_view script = R"(
  let i = 0;
  while (i < 3) {
    print_int(i);
    i = i + 1;
  }
  
    let tcb_ptr = rax;
    let flags_offset = 16;
    let target_addr = tcb_ptr + flags_offset;
    let raw_flags = read_u64(target_addr);
    let masked_flags = raw_flags & 0xFF00;
    print_int(read_u64(tcb_ptr));
    print_hex(masked_flags);
    print_hex(cr3);
    halt;
  )";

// A tiny register bank wide enough to cover the x86_64 DWARF indices used by
// this demo (only `rax`, index 0, is actually touched).
using register_bank = std::array<uint64_t, 17>;

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

// Human-readable mnemonic for each opcode, used below to dump the compiled
// program before it is executed.
constexpr microfmt::string_view opcode_name(opcode op) noexcept {
  switch (op) {
  case opcode::push_literal:
    return "push_literal";
  case opcode::load_register:
    return "load_register";
  case opcode::store_register:
    return "store_register";
  case opcode::read_u8:
    return "read_u8";
  case opcode::read_u16:
    return "read_u16";
  case opcode::read_u32:
    return "read_u32";
  case opcode::read_u64:
    return "read_u64";
  case opcode::store_u8:
    return "store_u8";
  case opcode::store_u16:
    return "store_u16";
  case opcode::store_u32:
    return "store_u32";
  case opcode::store_u64:
    return "store_u64";
  case opcode::work_ram_read_u8:
    return "work_ram_read_u8";
  case opcode::work_ram_read_u16:
    return "work_ram_read_u16";
  case opcode::work_ram_read_u32:
    return "work_ram_read_u32";
  case opcode::work_ram_read_u64:
    return "work_ram_read_u64";
  case opcode::work_ram_write_u8:
    return "work_ram_write_u8";
  case opcode::work_ram_write_u16:
    return "work_ram_write_u16";
  case opcode::work_ram_write_u32:
    return "work_ram_write_u32";
  case opcode::work_ram_write_u64:
    return "work_ram_write_u64";
  case opcode::print_int:
    return "print_int";
  case opcode::print_hex:
    return "print_hex";
  case opcode::print_char:
    return "print_char";
  case opcode::work_ram_print:
    return "work_ram_print";
  case opcode::add:
    return "add";
  case opcode::sub:
    return "sub";
  case opcode::mul:
    return "mul";
  case opcode::div:
    return "div";
  case opcode::mod:
    return "mod";
  case opcode::bit_and:
    return "bit_and";
  case opcode::bit_or:
    return "bit_or";
  case opcode::bit_xor:
    return "bit_xor";
  case opcode::shl:
    return "shl";
  case opcode::shr:
    return "shr";
  case opcode::and_not:
    return "and_not";
  case opcode::negate:
    return "negate";
  case opcode::bitwise_not:
    return "bitwise_not";
  case opcode::sign_extend_8:
    return "sign_extend_8";
  case opcode::sign_extend_16:
    return "sign_extend_16";
  case opcode::sign_extend_32:
    return "sign_extend_32";
  case opcode::equal:
    return "equal";
  case opcode::not_equal:
    return "not_equal";
  case opcode::less_than:
    return "less_than";
  case opcode::greater_than:
    return "greater_than";
  case opcode::less_or_equal:
    return "less_or_equal";
  case opcode::greater_or_equal:
    return "greater_or_equal";
  case opcode::dup:
    return "dup";
  case opcode::drop:
    return "drop";
  case opcode::swap:
    return "swap";
  case opcode::over:
    return "over";
  case opcode::jump:
    return "jump";
  case opcode::branch_zero:
    return "branch_zero";
  case opcode::branch_not_zero:
    return "branch_not_zero";
  case opcode::halt:
    return "halt";
  }
  return "unknown";
}

} // namespace

int main() {
  auto out = microfmt::stdout_sink();
  microfmt::println(out, "=== vm_compiler script demo ===");
  microfmt::println(out, "\nScript:\n{}", script);

  instruction code[1024];
  vm_code_generator gen(code);

  microfmt::inspector::label_allocator::label_info label_infos[64];
  microfmt::inspector::label_allocator::patch_site patch_sites[64];

  variable_symbol symbols[8]{};
  variable_context vars(symbols);
  microfmt::inspector::label_allocator labels(label_infos, patch_sites);

  const auto arch = target_arch_traits::create<microfmt::gdb::tags::x86_64>();
  const auto result = vm_compiler::compile(script, gen, vars, arch, labels, microfmt::stderr_sink());
  if (!result.success) {
    microfmt::println(out, "compilation failed!");
    return 1;
  }

  const auto program = gen.program();
  microfmt::println(out, "Compiled {} instructions:", program.size());
  for (size_t i = 0; i < program.size(); ++i) {
    microfmt::println(out, "  {:>3}: {:<16} {}", i, opcode_name(program[i].op), program[i].operand);
  }

  register_bank regs{};
  std::byte reg_scratch[8]{};
  auto space = microfmt::address_space_ref::make<microfmt::fallible_local_space_tag>();
  auto reg_ctx = microfmt::make_register_context_ref<read_register, write_register>(regs, space, reg_scratch);

  uint64_t stack[8]{};
  uint8_t work_ram[64]{};

  microfmt::print(out, "\nOutput: ");
  auto exec_result = memory_vm_executor::execute(program, space, reg_ctx, stack, work_ram, out);
  microfmt::println(out, "\n(success={}, rax={})", exec_result.success, regs[microfmt::dwarf::x86_64::rax]);

  return exec_result.success ? 0 : 1;
}
