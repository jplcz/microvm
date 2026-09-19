// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "micro_vm_test_support.hpp"

#include <microvm/vm_code_gen.hpp>
#include <microvm/vm_label_allocator.hpp>
#include <microfmt/sinks/stdio.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace {

using microfmt::inspector::instruction;
using microfmt::inspector::label_allocator;
using microfmt::inspector::memory_vm_executor;
using microfmt::inspector::opcode;
using microfmt::inspector::vm_code_generator;
using microfmt_test::micro_vm_fixture;

TEST(VmLabelAllocator, CreateLabelAllocatesSequentialIds) {
  label_allocator::label_info labels[4]{};
  label_allocator::patch_site patches[4]{};
  label_allocator allocator(labels, patches);

  EXPECT_EQ(allocator.create_label(), 0U);
  EXPECT_EQ(allocator.create_label(), 1U);
  EXPECT_EQ(allocator.create_label(), 2U);
  EXPECT_EQ(allocator.label_count(), 3U);
}

TEST(VmLabelAllocator, CreateLabelOverflowReturnsSizeMax) {
  label_allocator::label_info labels[2]{};
  label_allocator::patch_site patches[1]{};
  label_allocator allocator(labels, patches);

  EXPECT_EQ(allocator.create_label(), 0U);
  EXPECT_EQ(allocator.create_label(), 1U);
  EXPECT_EQ(allocator.create_label(), SIZE_MAX); // Label table is full.
  EXPECT_EQ(allocator.label_count(), 2U);
}

TEST(VmLabelAllocator, DefineLabelSucceedsForKnownIdAndFailsForUnknownId) {
  label_allocator::label_info labels[2]{};
  label_allocator::patch_site patches[1]{};
  label_allocator allocator(labels, patches);

  const size_t label = allocator.create_label();
  EXPECT_TRUE(allocator.define_label(label, 42));
  EXPECT_FALSE(allocator.define_label(label + 1, 7)); // Never allocated.
}

TEST(VmLabelAllocator, RecordPatchTracksCountAndOverflows) {
  label_allocator::label_info labels[1]{};
  label_allocator::patch_site patches[1]{};
  label_allocator allocator(labels, patches);

  const size_t label = allocator.create_label();
  EXPECT_TRUE(allocator.record_patch(0, label));
  EXPECT_EQ(allocator.patch_count(), 1U);
  EXPECT_FALSE(allocator.record_patch(1, label)); // Patch table is full.
  EXPECT_EQ(allocator.patch_count(), 1U);
}

TEST(VmLabelAllocator, ResolvePatchesRewritesJumpOperands) {
  label_allocator::label_info labels[2]{};
  label_allocator::patch_site patches[2]{};
  label_allocator allocator(labels, patches);

  const size_t forward_label = allocator.create_label();
  const size_t back_label = allocator.create_label();

  instruction program[]{
      {opcode::jump, 0},        // 0: patched to jump to forward_label (index 2)
      {opcode::branch_zero, 0}, // 1: patched to jump to back_label (index 0)
      {opcode::halt, 0},        // 2: forward_label target
  };

  ASSERT_TRUE(allocator.record_patch(0, forward_label));
  ASSERT_TRUE(allocator.record_patch(1, back_label));
  ASSERT_TRUE(allocator.define_label(forward_label, 2));
  ASSERT_TRUE(allocator.define_label(back_label, 0));

  ASSERT_TRUE(allocator.resolve_patches(program));
  EXPECT_EQ(program[0].operand, 2U);
  EXPECT_EQ(program[1].operand, 0U);
}

TEST(VmLabelAllocator, ResolvePatchesFailsForUndefinedLabel) {
  label_allocator::label_info labels[1]{};
  label_allocator::patch_site patches[1]{};
  label_allocator allocator(labels, patches);

  const size_t label = allocator.create_label();
  instruction program[]{{opcode::jump, 0}, {opcode::halt, 0}};

  ASSERT_TRUE(allocator.record_patch(0, label));
  // `label` was never defined via define_label().
  EXPECT_FALSE(allocator.resolve_patches(program));
}

TEST(VmLabelAllocator, ResolvePatchesFailsForUnknownLabelId) {
  label_allocator::label_info labels[1]{};
  label_allocator::patch_site patches[1]{};
  label_allocator allocator(labels, patches);

  instruction program[]{{opcode::jump, 0}, {opcode::halt, 0}};

  ASSERT_TRUE(allocator.record_patch(0, 99)); // No label was ever allocated.
  EXPECT_FALSE(allocator.resolve_patches(program));
}

TEST(VmLabelAllocator, ResolvePatchesFailsForOutOfRangeInstructionIndex) {
  label_allocator::label_info labels[1]{};
  label_allocator::patch_site patches[1]{};
  label_allocator allocator(labels, patches);

  const size_t label = allocator.create_label();
  instruction program[]{{opcode::halt, 0}};

  ASSERT_TRUE(allocator.define_label(label, 0));
  ASSERT_TRUE(allocator.record_patch(5, label)); // Out of bounds for `program`.
  EXPECT_FALSE(allocator.resolve_patches(program));
}

// End-to-end integration: build a forward-jump program with
// `vm_code_generator`, resolve the jump target with `label_allocator`, and
// execute the result through `memory_vm_executor`.
TEST(VmLabelAllocator, IntegratesWithCodeGeneratorAndExecutor) {
  micro_vm_fixture fx;

  instruction code[16];
  vm_code_generator gen(code);

  label_allocator::label_info labels[2]{};
  label_allocator::patch_site patches[2]{};
  label_allocator allocator(labels, patches);
  const size_t skip_label = allocator.create_label();

  // if (0) { push(1) } push(99); halt
  gen.push(0);
  ASSERT_TRUE(allocator.record_patch(gen.current_offset(), skip_label));
  gen.branch_zero(0); // Placeholder operand; patched below.
  gen.push(1);        // Skipped when the condition is zero.
  ASSERT_TRUE(allocator.define_label(skip_label, gen.current_offset()));
  gen.push(99).halt();

  ASSERT_TRUE(allocator.resolve_patches(microfmt::span<instruction>(code, gen.size())));

  uint64_t stack[4]{};
  uint8_t work_ram[4]{};
  auto result = memory_vm_executor::execute(gen.program(), fx.space, fx.reg_ctx, stack, work_ram,
                                            microfmt::stdout_sink());
  ASSERT_TRUE(result.success);
  EXPECT_EQ(result.value, 99U);
}

} // namespace
