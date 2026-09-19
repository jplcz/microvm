// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <microfmt/microfmt.hpp>
#include <microvm/vm_code_gen.hpp>

namespace microfmt::inspector {

class label_allocator {
public:
  struct label_info {
    size_t target_ip;
    bool defined;
  };

  struct patch_site {
    size_t instruction_index;
    size_t label_id;
  };

  /**
   * @brief Constructs a zero-allocation label allocator using caller-owned scratch spans.
   * @param labels_buffer Span to store label definitions.
   * @param patches_buffer Span to store unresolved jump patch sites.
   */
  constexpr label_allocator(span<label_info> labels_buffer, span<patch_site> patches_buffer) noexcept
      : labels_(labels_buffer), patches_(patches_buffer), label_count_(0), patch_count_(0) {}

  /**
   * @brief Allocates a new unique label ID.
   */
  constexpr size_t create_label() noexcept {
    if (label_count_ >= labels_.size()) {
      return SIZE_MAX; // Label table overflow
    }
    size_t id = label_count_++;
    labels_[id] = label_info{0, false};
    return id;
  }

  /**
   * @brief Marks a label as defined at the current instruction pointer offset.
   */
  constexpr bool define_label(size_t label_id, size_t current_ip) noexcept {
    if (label_id >= label_count_) {
      return false;
    }
    labels_[label_id] = label_info{current_ip, true};
    return true;
  }

  /**
   * @brief Records a jump instruction that needs its target operand resolved later.
   */
  constexpr bool record_patch(size_t instruction_index, size_t label_id) noexcept {
    if (patch_count_ >= patches_.size()) {
      return false; // Patch table overflow
    }
    patches_[patch_count_++] = patch_site{instruction_index, label_id};
    return true;
  }

  /**
   * @brief Resolves all recorded patch sites against the compiled program instruction buffer.
   */
  constexpr bool resolve_patches(span<instruction> program) noexcept {
    for (size_t i = 0; i < patch_count_; ++i) {
      const auto &patch = patches_[i];
      if (patch.label_id >= label_count_ || !labels_[patch.label_id].defined) {
        return false; // Error: Use of undefined label
      }
      if (patch.instruction_index >= program.size()) {
        return false; // Error: Invalid instruction index
      }
      // Update the jump instruction's operand with the resolved target IP
      program[patch.instruction_index].operand = labels_[patch.label_id].target_ip;
    }
    return true;
  }

  [[nodiscard]] constexpr size_t label_count() const noexcept { return label_count_; }
  [[nodiscard]] constexpr size_t patch_count() const noexcept { return patch_count_; }

private:
  span<label_info> labels_;
  span<patch_site> patches_;
  size_t label_count_;
  size_t patch_count_;
};

} // namespace microfmt::inspector