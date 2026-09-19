// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <microfmt/microfmt.hpp>
#include <microfmt/inspector/address_space.hpp>
#include <microfmt/inspector/register_context.hpp>
#include <cstring>

namespace microfmt::inspector {

enum class opcode : uint8_t {
  // --- Literals & Registers ---
  push_literal,   // Push a 64-bit constant onto the stack
  load_register,  // Operand = register index (e.g., DWARF index). Push reg value.
  store_register, // Pop value from stack, write to register index specified by operand

  // --- Fault-Safe Target Memory Reads ---
  read_u8,  // Pop address, read target uint8_t, push zero-extended value
  read_u16, // Pop address, read target uint16_t, push zero-extended value
  read_u32, // Pop address, read target uint32_t, push zero-extended value
  read_u64, // Pop address, read target uint64_t, push value

  // --- Fault-Safe Target Memory Writes ---
  store_u8,  // Pop value, pop address; write uint8_t to target memory
  store_u16, // Pop value, pop address; write uint16_t to target memory
  store_u32, // Pop value, pop address; write uint32_t to target memory
  store_u64, // Pop value, pop address; write uint64_t to target memory

  // --- Scratch Work RAM Operations ---
  work_ram_read_u8,   // Pop offset, read uint8_t from work_ram, push zero-extended value
  work_ram_read_u16,  // Pop offset, read uint16_t from work_ram, push zero-extended value
  work_ram_read_u32,  // Pop offset, read uint32_t from work_ram, push zero-extended value
  work_ram_read_u64,  // Pop offset, read uint64_t from work_ram, push value
  work_ram_write_u8,  // Pop value, pop offset; write uint8_t to work_ram
  work_ram_write_u16, // Pop value, pop offset; write uint16_t to work_ram
  work_ram_write_u32, // Pop value, pop offset; write uint32_t to work_ram
  work_ram_write_u64, // Pop value, pop offset; write uint64_t to work_ram

  // --- Sink Printing Operations ---
  print_int,      // Pop value, format and write as signed decimal to sink
  print_hex,      // Pop value, format and write as 0x hex to sink
  print_char,     // Pop value, write single ASCII character to sink
  work_ram_print, // Pop length, pop offset; write ASCII string from work_ram to sink

  // --- Arithmetic & Division ---
  add, // Pop a, b; push (b + a)
  sub, // Pop a, b; push (b - a)
  mul, // Pop a, b; push (b * a)
  div, // Pop a, b; if a == 0 return fault, else push (b / a)
  mod, // Pop a, b; if a == 0 return fault, else push (b % a)

  // --- Bitwise & Shifts ---
  bit_and, // Pop a, b; push (b & a)
  bit_or,  // Pop a, b; push (b | a)
  bit_xor, // Pop a, b; push (b ^ a)
  shl,     // Pop a (shift count), b (value); if a >= 64 return fault, else push (b << a)
  shr,     // Pop a (shift count), b (value); if a >= 64 return fault, else push (b >> a)
  and_not, // Pop a, b; push (b & ~a)

  // --- Unary & Sign Extension ---
  negate,         // Pop a; push (-a)
  bitwise_not,    // Pop a; push (~a)
  sign_extend_8,  // Pop a; sign-extend 8-bit integer to 64 bits
  sign_extend_16, // Pop a; sign-extend 16-bit integer to 64 bits
  sign_extend_32, // Pop a; sign-extend 32-bit integer to 64 bits

  // --- Comparisons ---
  equal,            // Pop a, b; push (b == a ? 1 : 0)
  not_equal,        // Pop a, b; push (b != a ? 1 : 0)
  less_than,        // Pop a, b; push (b < a ? 1 : 0)
  greater_than,     // Pop a, b; push (b > a ? 1 : 0)
  less_or_equal,    // Pop a, b; push (b <= a ? 1 : 0)
  greater_or_equal, // Pop a, b; push (b >= a ? 1 : 0)

  // --- Stack Manipulation ---
  dup,  // Duplicate top stack element: [a] -> [a, a]
  drop, // Discard top stack element: [a] -> []
  swap, // Swap top two stack elements: [a, b] -> [b, a]
  over, // Copy second stack element to top: [a, b] -> [a, b, a]

  // --- Flow Control ---
  jump,            // Unconditional jump: set instruction pointer (IP) to operand
  branch_zero,     // Pop condition; if 0, set IP to operand
  branch_not_zero, // Pop condition; if != 0, set IP to operand
  halt             // Stop execution successfully
};

struct instruction {
  opcode op;
  uint64_t operand;
};

class memory_vm_executor {
public:
  struct result {
    uint64_t value;
    bool success;
  };

  /**
   * @brief Executes a zero-allocation VM program against target memory, registers, and an output sink.
   * @param program Instruction stream span.
   * @param space Address space reference for memory reads/writes.
   * @param regs Register context reference for register reads/writes.
   * @param evaluation_stack Caller-owned scratch span used as the VM evaluation stack.
   * @param work_ram Caller-owned scratch RAM span for script usage.
   * @param output_sink Type-erased sink for streaming prints.
   * @param max_steps Maximum number of byte codes to execute (watchdog safeguard).
   */
  static result execute(span<const instruction> program, address_space_ref space, register_context_ref regs,
                        span<uint64_t> evaluation_stack, span<uint8_t> work_ram, sink output_sink,
                        size_t max_steps = 10000) noexcept {
    if (evaluation_stack.empty() || program.empty()) {
      return {0, false};
    }

    int sp = -1;
    const size_t stack_capacity = evaluation_stack.size();

    auto push = [&](uint64_t val) -> bool {
      if (static_cast<size_t>(sp + 1) >= stack_capacity) {
        return false;
      }
      ++sp;
      evaluation_stack[static_cast<size_t>(sp)] = val;
      return true;
    };

    auto pop = [&]() -> uint64_t {
      const int index = sp;
      --sp;
      return evaluation_stack[(size_t)index];
    };

    size_t ip = 0;
    size_t steps = 0;
    const size_t program_len = program.size();

    while (ip < program_len) {
      if (++steps > max_steps) {
        return {0, false}; // Watchdog limit exceeded
      }

      const auto &inst = program[ip++];
      switch (inst.op) {
      case opcode::push_literal:
        if (!push(inst.operand))
          return {0, false};
        break;

      case opcode::load_register: {
        uint64_t reg_val = 0;
        if (!regs.read<uint64_t>(static_cast<uint32_t>(inst.operand), reg_val)) {
          return {0, false};
        }
        if (!push(reg_val))
          return {0, false};
        break;
      }

      case opcode::store_register: {
        if (sp < 0)
          return {0, false};
        uint64_t val = pop();
        if (!regs.write(static_cast<uint32_t>(inst.operand), val)) {
          return {0, false};
        }
        break;
      }

      case opcode::read_u8: {
        if (sp < 0)
          return {0, false};
        uint64_t addr = pop();
        auto res = space.read<uint8_t>(addr);
        if (!res.has_value())
          return {0, false};
        if (!push(static_cast<uint64_t>(*res)))
          return {0, false};
        break;
      }

      case opcode::read_u16: {
        if (sp < 0)
          return {0, false};
        uint64_t addr = pop();
        auto res = space.read<uint16_t>(addr);
        if (!res.has_value())
          return {0, false};
        if (!push(static_cast<uint64_t>(*res)))
          return {0, false};
        break;
      }

      case opcode::read_u32: {
        if (sp < 0)
          return {0, false};
        uint64_t addr = pop();
        auto res = space.read<uint32_t>(addr);
        if (!res.has_value())
          return {0, false};
        if (!push(static_cast<uint64_t>(*res)))
          return {0, false};
        break;
      }

      case opcode::read_u64: {
        if (sp < 0)
          return {0, false};
        uint64_t addr = pop();
        auto res = space.read<uint64_t>(addr);
        if (!res.has_value())
          return {0, false};
        if (!push(*res))
          return {0, false};
        break;
      }

      case opcode::store_u8: {
        if (sp < 1)
          return {0, false};
        uint64_t val = pop();
        uint64_t addr = pop();
        auto res = space.write<uint8_t>(addr, static_cast<uint8_t>(val));
        if (!res.has_value())
          return {0, false};
        break;
      }

      case opcode::store_u16: {
        if (sp < 1)
          return {0, false};
        uint64_t val = pop();
        uint64_t addr = pop();
        auto res = space.write<uint16_t>(addr, static_cast<uint16_t>(val));
        if (!res.has_value())
          return {0, false};
        break;
      }

      case opcode::store_u32: {
        if (sp < 1)
          return {0, false};
        uint64_t val = pop();
        uint64_t addr = pop();
        auto res = space.write<uint32_t>(addr, static_cast<uint32_t>(val));
        if (!res.has_value())
          return {0, false};
        break;
      }

      case opcode::store_u64: {
        if (sp < 1)
          return {0, false};
        uint64_t val = pop();
        uint64_t addr = pop();
        auto res = space.write<uint64_t>(addr, val);
        if (!res.has_value())
          return {0, false};
        break;
      }

      // --- Work RAM Read/Write Operations ---
      case opcode::work_ram_read_u8: {
        if (sp < 0)
          return {0, false};
        size_t offset = static_cast<size_t>(pop());
        if (offset + sizeof(uint8_t) > work_ram.size())
          return {0, false};
        uint8_t val = 0;
        std::memcpy(&val, work_ram.data() + offset, sizeof(uint8_t));
        if (!push(static_cast<uint64_t>(val)))
          return {0, false};
        break;
      }

      case opcode::work_ram_read_u16: {
        if (sp < 0)
          return {0, false};
        size_t offset = static_cast<size_t>(pop());
        if (offset + sizeof(uint16_t) > work_ram.size())
          return {0, false};
        uint16_t val = 0;
        std::memcpy(&val, work_ram.data() + offset, sizeof(uint16_t));
        if (!push(static_cast<uint64_t>(val)))
          return {0, false};
        break;
      }

      case opcode::work_ram_read_u32: {
        if (sp < 0)
          return {0, false};
        size_t offset = static_cast<size_t>(pop());
        if (offset + sizeof(uint32_t) > work_ram.size())
          return {0, false};
        uint32_t val = 0;
        std::memcpy(&val, work_ram.data() + offset, sizeof(uint32_t));
        if (!push(static_cast<uint64_t>(val)))
          return {0, false};
        break;
      }

      case opcode::work_ram_read_u64: {
        if (sp < 0)
          return {0, false};
        size_t offset = static_cast<size_t>(pop());
        if (offset + sizeof(uint64_t) > work_ram.size())
          return {0, false};
        uint64_t val = 0;
        std::memcpy(&val, work_ram.data() + offset, sizeof(uint64_t));
        if (!push(val))
          return {0, false};
        break;
      }

      case opcode::work_ram_write_u8: {
        if (sp < 1)
          return {0, false};
        uint64_t val = pop();
        size_t offset = static_cast<size_t>(pop());
        if (offset + sizeof(uint8_t) > work_ram.size())
          return {0, false};
        uint8_t b = static_cast<uint8_t>(val);
        std::memcpy(work_ram.data() + offset, &b, sizeof(uint8_t));
        break;
      }

      case opcode::work_ram_write_u16: {
        if (sp < 1)
          return {0, false};
        uint64_t val = pop();
        size_t offset = static_cast<size_t>(pop());
        if (offset + sizeof(uint16_t) > work_ram.size())
          return {0, false};
        uint16_t h = static_cast<uint16_t>(val);
        std::memcpy(work_ram.data() + offset, &h, sizeof(uint16_t));
        break;
      }

      case opcode::work_ram_write_u32: {
        if (sp < 1)
          return {0, false};
        uint64_t val = pop();
        size_t offset = static_cast<size_t>(pop());
        if (offset + sizeof(uint32_t) > work_ram.size())
          return {0, false};
        uint32_t w = static_cast<uint32_t>(val);
        std::memcpy(work_ram.data() + offset, &w, sizeof(uint32_t));
        break;
      }

      case opcode::work_ram_write_u64: {
        if (sp < 1)
          return {0, false};
        uint64_t val = pop();
        size_t offset = static_cast<size_t>(pop());
        if (offset + sizeof(uint64_t) > work_ram.size())
          return {0, false};
        std::memcpy(work_ram.data() + offset, &val, sizeof(uint64_t));
        break;
      }

      // --- Sink Printing Instructions ---
      case opcode::print_int: {
        if (sp < 0)
          return {0, false};
        uint64_t val = pop();
        format_to(output_sink, "{}", static_cast<int64_t>(val));
        break;
      }

      case opcode::print_hex: {
        if (sp < 0)
          return {0, false};
        uint64_t val = pop();
        format_to(output_sink, "0x{:x}", val);
        break;
      }

      case opcode::print_char: {
        if (sp < 0)
          return {0, false};
        uint64_t val = pop();
        format_to(output_sink, "{}", static_cast<char>(val & 0xFF));
        break;
      }

      case opcode::work_ram_print: {
        if (sp < 1)
          return {0, false};
        size_t length = static_cast<size_t>(pop());
        size_t offset = static_cast<size_t>(pop());
        if (offset + length > work_ram.size())
          return {0, false};
        string_view sv(reinterpret_cast<const char *>(work_ram.data() + offset), length);
        format_to(output_sink, "{}", sv);
        break;
      }

      // --- Arithmetic ---
      case opcode::add: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b + a))
          return {0, false};
        break;
      }

      case opcode::sub: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b - a))
          return {0, false};
        break;
      }

      case opcode::mul: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b * a))
          return {0, false};
        break;
      }

      case opcode::div: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (a == 0)
          return {0, false};
        if (!push(b / a))
          return {0, false};
        break;
      }

      case opcode::mod: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (a == 0)
          return {0, false};
        if (!push(b % a))
          return {0, false};
        break;
      }

      // --- Bitwise & Shifts ---
      case opcode::bit_and: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b & a))
          return {0, false};
        break;
      }

      case opcode::bit_or: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b | a))
          return {0, false};
        break;
      }

      case opcode::bit_xor: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b ^ a))
          return {0, false};
        break;
      }

      case opcode::shl: {
        if (sp < 1)
          return {0, false};
        uint64_t count = pop();
        uint64_t val = pop();
        if (count >= 64)
          return {0, false};
        if (!push(val << count))
          return {0, false};
        break;
      }

      case opcode::shr: {
        if (sp < 1)
          return {0, false};
        uint64_t count = pop();
        uint64_t val = pop();
        if (count >= 64)
          return {0, false};
        if (!push(val >> count))
          return {0, false};
        break;
      }

      case opcode::and_not: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b & ~a))
          return {0, false};
        break;
      }

      // --- Unary & Sign Extension ---
      case opcode::negate: {
        if (sp < 0)
          return {0, false};
        evaluation_stack[(size_t)sp] = static_cast<uint64_t>(-static_cast<int64_t>(evaluation_stack[(size_t)sp]));
        break;
      }

      case opcode::bitwise_not: {
        if (sp < 0)
          return {0, false};
        evaluation_stack[(size_t)sp] = ~evaluation_stack[(size_t)sp];
        break;
      }

      case opcode::sign_extend_8: {
        if (sp < 0)
          return {0, false};
        int8_t val = static_cast<int8_t>(evaluation_stack[(size_t)sp] & 0xFF);
        evaluation_stack[(size_t)sp] = static_cast<uint64_t>(static_cast<int64_t>(val));
        break;
      }

      case opcode::sign_extend_16: {
        if (sp < 0)
          return {0, false};
        int16_t val = static_cast<int16_t>(evaluation_stack[(size_t)sp] & 0xFFFF);
        evaluation_stack[(size_t)sp] = static_cast<uint64_t>(static_cast<int64_t>(val));
        break;
      }

      case opcode::sign_extend_32: {
        if (sp < 0)
          return {0, false};
        int32_t val = static_cast<int32_t>(evaluation_stack[(size_t)sp] & 0xFFFFFFFF);
        evaluation_stack[(size_t)sp] = static_cast<uint64_t>(static_cast<int64_t>(val));
        break;
      }

      // --- Comparisons ---
      case opcode::equal: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b == a ? 1 : 0))
          return {0, false};
        break;
      }

      case opcode::not_equal: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b != a ? 1 : 0))
          return {0, false};
        break;
      }

      case opcode::less_than: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b < a ? 1 : 0))
          return {0, false};
        break;
      }

      case opcode::greater_than: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b > a ? 1 : 0))
          return {0, false};
        break;
      }

      case opcode::less_or_equal: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b <= a ? 1 : 0))
          return {0, false};
        break;
      }

      case opcode::greater_or_equal: {
        if (sp < 1)
          return {0, false};
        uint64_t a = pop();
        uint64_t b = pop();
        if (!push(b >= a ? 1 : 0))
          return {0, false};
        break;
      }

      // --- Stack Manipulation ---
      case opcode::dup: {
        if (sp < 0)
          return {0, false};
        if (!push(evaluation_stack[(size_t)sp]))
          return {0, false};
        break;
      }

      case opcode::drop: {
        if (sp < 0)
          return {0, false};
        pop();
        break;
      }

      case opcode::swap: {
        if (sp < 1)
          return {0, false};
        std::swap(evaluation_stack[(size_t)sp], evaluation_stack[(size_t)sp - 1]);
        break;
      }

      case opcode::over: {
        if (sp < 1)
          return {0, false};
        if (!push(evaluation_stack[(size_t)sp - 1]))
          return {0, false};
        break;
      }

      // --- Flow Control ---
      case opcode::jump:
        if (inst.operand >= program_len)
          return {0, false};
        ip = static_cast<size_t>(inst.operand);
        break;

      case opcode::branch_zero: {
        if (sp < 0)
          return {0, false};
        uint64_t cond = pop();
        if (cond == 0) {
          if (inst.operand >= program_len)
            return {0, false};
          ip = static_cast<size_t>(inst.operand);
        }
        break;
      }

      case opcode::branch_not_zero: {
        if (sp < 0)
          return {0, false};
        uint64_t cond = pop();
        if (cond != 0) {
          if (inst.operand >= program_len)
            return {0, false};
          ip = static_cast<size_t>(inst.operand);
        }
        break;
      }

      case opcode::halt:
        return {sp >= 0 ? evaluation_stack[(size_t)sp] : 0, true};
      }
    }

    return {sp >= 0 ? evaluation_stack[(size_t)sp] : 0, (size_t)sp >= 0};
  }
};

} // namespace microfmt::inspector