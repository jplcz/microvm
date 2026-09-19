// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <microfmt/microfmt.hpp>
#include <microvm/micro_vm.hpp>

namespace microfmt::inspector {

/**
 * @brief Zero-allocation programmatic code generator (bytecode emitter).
 * Builds instruction streams safely into a caller-owned buffer span for all opcodes.
 */
class vm_code_generator {
public:
  explicit constexpr vm_code_generator(span<instruction> buffer) noexcept : buffer_(buffer), index_(0) {}

  [[nodiscard]] constexpr size_t size() const noexcept { return index_; }
  [[nodiscard]] constexpr bool has_space() const noexcept { return index_ < buffer_.size(); }

  // --- Literals & Registers ---
  constexpr vm_code_generator &push(uint64_t val) noexcept {
    emit(opcode::push_literal, val);
    return *this;
  }

  constexpr vm_code_generator &load_reg(uint32_t reg_idx) noexcept {
    emit(opcode::load_register, reg_idx);
    return *this;
  }

  constexpr vm_code_generator &store_reg(uint32_t reg_idx) noexcept {
    emit(opcode::store_register, reg_idx);
    return *this;
  }

  // --- Fault-Safe Target Memory Reads ---
  constexpr vm_code_generator &read_u8() noexcept {
    emit(opcode::read_u8, 0);
    return *this;
  }
  constexpr vm_code_generator &read_u16() noexcept {
    emit(opcode::read_u16, 0);
    return *this;
  }
  constexpr vm_code_generator &read_u32() noexcept {
    emit(opcode::read_u32, 0);
    return *this;
  }
  constexpr vm_code_generator &read_u64() noexcept {
    emit(opcode::read_u64, 0);
    return *this;
  }

  // --- Fault-Safe Target Memory Writes ---
  constexpr vm_code_generator &store_u8() noexcept {
    emit(opcode::store_u8, 0);
    return *this;
  }
  constexpr vm_code_generator &store_u16() noexcept {
    emit(opcode::store_u16, 0);
    return *this;
  }
  constexpr vm_code_generator &store_u32() noexcept {
    emit(opcode::store_u32, 0);
    return *this;
  }
  constexpr vm_code_generator &store_u64() noexcept {
    emit(opcode::store_u64, 0);
    return *this;
  }

  // --- Scratch Work RAM Operations ---
  constexpr vm_code_generator &work_ram_read_u8() noexcept {
    emit(opcode::work_ram_read_u8, 0);
    return *this;
  }
  constexpr vm_code_generator &work_ram_read_u16() noexcept {
    emit(opcode::work_ram_read_u16, 0);
    return *this;
  }
  constexpr vm_code_generator &work_ram_read_u32() noexcept {
    emit(opcode::work_ram_read_u32, 0);
    return *this;
  }
  constexpr vm_code_generator &work_ram_read_u64() noexcept {
    emit(opcode::work_ram_read_u64, 0);
    return *this;
  }
  constexpr vm_code_generator &work_ram_write_u8() noexcept {
    emit(opcode::work_ram_write_u8, 0);
    return *this;
  }
  constexpr vm_code_generator &work_ram_write_u16() noexcept {
    emit(opcode::work_ram_write_u16, 0);
    return *this;
  }
  constexpr vm_code_generator &work_ram_write_u32() noexcept {
    emit(opcode::work_ram_write_u32, 0);
    return *this;
  }
  constexpr vm_code_generator &work_ram_write_u64() noexcept {
    emit(opcode::work_ram_write_u64, 0);
    return *this;
  }

  // --- Sink Printing Operations ---
  constexpr vm_code_generator &print_int() noexcept {
    emit(opcode::print_int, 0);
    return *this;
  }
  constexpr vm_code_generator &print_hex() noexcept {
    emit(opcode::print_hex, 0);
    return *this;
  }
  constexpr vm_code_generator &print_char() noexcept {
    emit(opcode::print_char, 0);
    return *this;
  }
  constexpr vm_code_generator &work_ram_print() noexcept {
    emit(opcode::work_ram_print, 0);
    return *this;
  }

  // --- Arithmetic & Division ---
  constexpr vm_code_generator &add() noexcept {
    emit(opcode::add, 0);
    return *this;
  }
  constexpr vm_code_generator &sub() noexcept {
    emit(opcode::sub, 0);
    return *this;
  }
  constexpr vm_code_generator &mul() noexcept {
    emit(opcode::mul, 0);
    return *this;
  }
  constexpr vm_code_generator &div() noexcept {
    emit(opcode::div, 0);
    return *this;
  }
  constexpr vm_code_generator &mod() noexcept {
    emit(opcode::mod, 0);
    return *this;
  }

  // --- Bitwise & Shifts ---
  constexpr vm_code_generator &bit_and() noexcept {
    emit(opcode::bit_and, 0);
    return *this;
  }
  constexpr vm_code_generator &bit_or() noexcept {
    emit(opcode::bit_or, 0);
    return *this;
  }
  constexpr vm_code_generator &bit_xor() noexcept {
    emit(opcode::bit_xor, 0);
    return *this;
  }
  constexpr vm_code_generator &shl() noexcept {
    emit(opcode::shl, 0);
    return *this;
  }
  constexpr vm_code_generator &shr() noexcept {
    emit(opcode::shr, 0);
    return *this;
  }
  constexpr vm_code_generator &and_not() noexcept {
    emit(opcode::and_not, 0);
    return *this;
  }

  // --- Unary & Sign Extension ---
  constexpr vm_code_generator &negate() noexcept {
    emit(opcode::negate, 0);
    return *this;
  }
  constexpr vm_code_generator &bitwise_not() noexcept {
    emit(opcode::bitwise_not, 0);
    return *this;
  }
  constexpr vm_code_generator &sign_extend_8() noexcept {
    emit(opcode::sign_extend_8, 0);
    return *this;
  }
  constexpr vm_code_generator &sign_extend_16() noexcept {
    emit(opcode::sign_extend_16, 0);
    return *this;
  }
  constexpr vm_code_generator &sign_extend_32() noexcept {
    emit(opcode::sign_extend_32, 0);
    return *this;
  }

  // --- Comparisons ---
  constexpr vm_code_generator &equal() noexcept {
    emit(opcode::equal, 0);
    return *this;
  }
  constexpr vm_code_generator &not_equal() noexcept {
    emit(opcode::not_equal, 0);
    return *this;
  }
  constexpr vm_code_generator &less_than() noexcept {
    emit(opcode::less_than, 0);
    return *this;
  }
  constexpr vm_code_generator &greater_than() noexcept {
    emit(opcode::greater_than, 0);
    return *this;
  }
  constexpr vm_code_generator &less_or_equal() noexcept {
    emit(opcode::less_or_equal, 0);
    return *this;
  }
  constexpr vm_code_generator &greater_or_equal() noexcept {
    emit(opcode::greater_or_equal, 0);
    return *this;
  }

  // --- Stack Manipulation ---
  constexpr vm_code_generator &dup() noexcept {
    emit(opcode::dup, 0);
    return *this;
  }
  constexpr vm_code_generator &drop() noexcept {
    emit(opcode::drop, 0);
    return *this;
  }
  constexpr vm_code_generator &swap() noexcept {
    emit(opcode::swap, 0);
    return *this;
  }
  constexpr vm_code_generator &over() noexcept {
    emit(opcode::over, 0);
    return *this;
  }

  // --- Flow Control ---
  [[nodiscard]] constexpr size_t current_offset() const noexcept { return index_; }

  constexpr vm_code_generator &jump(size_t target_ip) noexcept {
    emit(opcode::jump, target_ip);
    return *this;
  }

  constexpr vm_code_generator &branch_zero(size_t target_ip) noexcept {
    emit(opcode::branch_zero, target_ip);
    return *this;
  }

  constexpr vm_code_generator &branch_not_zero(size_t target_ip) noexcept {
    emit(opcode::branch_not_zero, target_ip);
    return *this;
  }

  constexpr vm_code_generator &halt() noexcept {
    emit(opcode::halt, 0);
    return *this;
  }

  /**
   * @brief Returns a subspan containing the successfully generated program.
   */
  [[nodiscard]] constexpr span<instruction> program() const & noexcept MICROFMT_LIFETIMEBOUND {
    // Construct the const-qualified span directly (rather than implicitly
    // converting a `subspan()` result) to avoid a false-positive
    // `-Wreturn-stack-address`: the converting constructor's lifetimebound
    // parameter otherwise ties the result to the `subspan()` temporary
    // itself, even though only its (non-owning) pointer/size are copied.
    return span<instruction>(buffer_.data(), index_);
  }

  span<const instruction> program() const && noexcept = delete;

  [[nodiscard]] constexpr bool has_overflowed() const noexcept { return overflowed_; }

private:
  constexpr bool emit(opcode op, uint64_t operand) noexcept {
    if (index_ >= buffer_.size()) {
      overflowed_ = true;
      return false;
    }
    buffer_[index_++] = instruction{op, operand};
    return true;
  }

  span<instruction> buffer_;
  size_t index_;
  bool overflowed_ = false;
};

} // namespace microfmt::inspector