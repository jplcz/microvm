// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

/** @file micro_vm_test_support.hpp
 * @brief Shared test fixtures for `micro_vm.hpp`, `vm_code_gen.hpp`, and
 * `vm_label_allocator.hpp` unit tests.
 *
 * Bundles a minimal fixed-size register bank, a stateless local address
 * space, and a ready-to-use `microfmt::register_context_ref` so individual
 * test files do not have to redefine the same register-context plumbing.
 */

#pragma once

#include <microfmt/inspector/register_context.hpp>
#include <microvm/micro_vm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace microfmt_test {

// Fixed-size register bank used by the test register-context callbacks.
// Registers are stored as uint64_t so their width matches the VM's
// load_register/store_register operand size.
using register_bank = std::array<uint64_t, 4>;

inline bool read_register(const register_bank *state, microfmt::address_space_ref, uint32_t index, void *dest,
                          size_t size) noexcept {
  if (!state || size != sizeof(uint64_t) || index >= state->size())
    return false;
  std::memcpy(dest, &(*state)[index], sizeof(uint64_t));
  return true;
}

inline bool write_register(register_bank *state, microfmt::address_space_ref, uint32_t index, const void *src,
                           size_t size) noexcept {
  if (!state || size != sizeof(uint64_t) || index >= state->size())
    return false;
  std::memcpy(&(*state)[index], src, sizeof(uint64_t));
  return true;
}

// Convenience fixture bundling an address space, register bank, and scratch
// buffer needed by every `microfmt::inspector::memory_vm_executor::execute`
// call.
struct micro_vm_fixture {
  register_bank regs{};
  std::byte scratch[8]{};
  microfmt::address_space_ref space = microfmt::address_space_ref::make<microfmt::local_space_tag>();
  microfmt::register_context_ref reg_ctx =
      microfmt::make_register_context_ref<read_register, write_register>(regs, space, scratch);
};

} // namespace microfmt_test
