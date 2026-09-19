// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <microfmt/lifetime.hpp>
#include <microfmt/microfmt.hpp>

namespace microfmt::inspector {

struct variable_symbol {
  string_view name;
  size_t work_ram_offset;
};

class variable_context {
public:
  /**
   * @brief Constructs a variable context backed by a caller-owned symbol buffer.
   * @param symbol_buffer Storage for allocated variable symbols; must outlive
   * this `variable_context`, whose lookups and allocations borrow from it.
   */
  constexpr explicit variable_context(
      span<variable_symbol> symbol_buffer MICROFMT_LIFETIMEBOUND
          MICROFMT_LIFETIME_CAPTURE_BY_THIS) noexcept
      : symbols_(symbol_buffer), count_(0), current_offset_(0) {}

  /**
   * @brief Looks up a variable or allocates a new slot in work_ram if it doesn't exist.
   * @param name Variable name; on allocation it is stored by value inside this
   * context, so its underlying character storage must outlive the context.
   * @param out_offset Set to the variable's `work_ram` byte offset on success.
   * @param size Slot size in bytes to reserve when allocating a new variable.
   * @return `true` on success (existing or newly allocated slot); `false` if
   * the symbol table or `work_ram` budget is exhausted.
   */
  constexpr bool get_or_allocate(
      string_view name MICROFMT_LIFETIMEBOUND
          MICROFMT_LIFETIME_CAPTURE_BY_THIS,
      size_t &out_offset, size_t size = 8) noexcept {
    // Search existing symbols
    for (size_t i = 0; i < count_; ++i) {
      if (symbols_[i].name == name) {
        out_offset = symbols_[i].work_ram_offset;
        return true;
      }
    }

    // Allocate new slot if space permits (aligning to 8 bytes)
    size_t aligned_offset = (current_offset_ + 7) & ~7ULL;
    if (count_ >= symbols_.size() || (aligned_offset + size) > 1024 /* max work ram guard */) {
      return false; // Symbol table or work_ram overflow
    }

    symbols_[count_] = variable_symbol{name, aligned_offset};
    out_offset = aligned_offset;
    count_++;
    current_offset_ = aligned_offset + size;
    return true;
  }

  constexpr void reset() noexcept {
    count_ = 0;
    current_offset_ = 0;
  }

private:
  span<variable_symbol> symbols_;
  size_t count_;
  size_t current_offset_;
};

} // namespace microfmt::inspector