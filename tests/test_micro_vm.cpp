// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "micro_vm_test_support.hpp"

#include <microvm/micro_vm.hpp>
#include <microfmt/sinks/container_sink.hpp>
#include <microfmt/sinks/stdio.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace {

using microfmt::inspector::instruction;
using microfmt::inspector::memory_vm_executor;
using microfmt::inspector::opcode;
using microfmt_test::micro_vm_fixture;

TEST(MicroVm, EmptyProgramFails) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  auto result = memory_vm_executor::execute({}, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  EXPECT_FALSE(result.success);
}

TEST(MicroVm, EmptyEvaluationStackFails) {
  micro_vm_fixture fx;
  instruction program[] = {{opcode::push_literal, 1}, {opcode::halt, 0}};
  uint8_t work_ram[4]{};

  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, microfmt::span<uint64_t>{}, work_ram,
                                            microfmt::stdout_sink());
  EXPECT_FALSE(result.success);
}

TEST(MicroVm, PushLiteralAndHaltReturnsTopOfStack) {
  micro_vm_fixture fx;
  instruction program[] = {{opcode::push_literal, 42}, {opcode::halt, 0}};
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(result.success);
  EXPECT_EQ(result.value, 42U);
}

TEST(MicroVm, HaltOnEmptyStackReturnsZero) {
  micro_vm_fixture fx;
  instruction program[] = {{opcode::halt, 0}};
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(result.success);
  EXPECT_EQ(result.value, 0U);
}

TEST(MicroVm, StackOverflowFails) {
  micro_vm_fixture fx;
  instruction program[] = {{opcode::push_literal, 1}, {opcode::push_literal, 2}, {opcode::halt, 0}};
  uint64_t stack[1]{}; // Only room for a single literal.
  uint8_t work_ram[4]{};

  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  EXPECT_FALSE(result.success);
}

TEST(MicroVm, StepWatchdogAbortsInfiniteLoop) {
  micro_vm_fixture fx;
  // An unconditional jump back to itself never reaches `halt`.
  instruction program[] = {{opcode::jump, 0}};
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink(),
                                            /*max_steps=*/16);
  EXPECT_FALSE(result.success);
}

TEST(MicroVm, JumpOutOfBoundsFails) {
  micro_vm_fixture fx;
  instruction program[] = {{opcode::jump, 99}};
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  EXPECT_FALSE(result.success);
}

TEST(MicroVm, ArithmeticOperationsComputeInStackOrder) {
  micro_vm_fixture fx;
  // (10 - 3) => 7
  instruction sub_program[] = {
      {opcode::push_literal, 10}, {opcode::push_literal, 3}, {opcode::sub, 0}, {opcode::halt, 0}};
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};
  auto sub_result =
      memory_vm_executor::execute(sub_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(sub_result.success);
  EXPECT_EQ(sub_result.value, 7U);

  // 6 * 7 => 42
  instruction mul_program[] = {
      {opcode::push_literal, 6}, {opcode::push_literal, 7}, {opcode::mul, 0}, {opcode::halt, 0}};
  auto mul_result =
      memory_vm_executor::execute(mul_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(mul_result.success);
  EXPECT_EQ(mul_result.value, 42U);

  // 20 / 6 => 3, 20 % 6 => 2
  instruction div_program[] = {
      {opcode::push_literal, 20}, {opcode::push_literal, 6}, {opcode::div, 0}, {opcode::halt, 0}};
  auto div_result =
      memory_vm_executor::execute(div_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(div_result.success);
  EXPECT_EQ(div_result.value, 3U);

  instruction mod_program[] = {
      {opcode::push_literal, 20}, {opcode::push_literal, 6}, {opcode::mod, 0}, {opcode::halt, 0}};
  auto mod_result =
      memory_vm_executor::execute(mod_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(mod_result.success);
  EXPECT_EQ(mod_result.value, 2U);
}

TEST(MicroVm, DivideAndModuloByZeroFault) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  instruction div_program[] = {
      {opcode::push_literal, 5}, {opcode::push_literal, 0}, {opcode::div, 0}, {opcode::halt, 0}};
  EXPECT_FALSE(
      memory_vm_executor::execute(div_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).success);

  instruction mod_program[] = {
      {opcode::push_literal, 5}, {opcode::push_literal, 0}, {opcode::mod, 0}, {opcode::halt, 0}};
  EXPECT_FALSE(
      memory_vm_executor::execute(mod_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).success);
}

TEST(MicroVm, BitwiseAndShiftOperations) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  instruction and_program[] = {
      {opcode::push_literal, 0b1100}, {opcode::push_literal, 0b1010}, {opcode::bit_and, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(and_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      0b1000U);

  instruction or_program[] = {
      {opcode::push_literal, 0b1100}, {opcode::push_literal, 0b0010}, {opcode::bit_or, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(or_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      0b1110U);

  instruction xor_program[] = {
      {opcode::push_literal, 0b1100}, {opcode::push_literal, 0b1010}, {opcode::bit_xor, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(xor_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      0b0110U);

  instruction and_not_program[] = {
      {opcode::push_literal, 0b1100}, {opcode::push_literal, 0b1010}, {opcode::and_not, 0}, {opcode::halt, 0}};
  EXPECT_EQ(memory_vm_executor::execute(and_not_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink())
                .value,
            0b0100U);

  instruction shl_program[] = {
      {opcode::push_literal, 1}, {opcode::push_literal, 4}, {opcode::shl, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(shl_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      16U);

  instruction shr_program[] = {
      {opcode::push_literal, 16}, {opcode::push_literal, 4}, {opcode::shr, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(shr_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      1U);

  instruction shift_overflow_program[] = {
      {opcode::push_literal, 1}, {opcode::push_literal, 64}, {opcode::shl, 0}, {opcode::halt, 0}};
  EXPECT_FALSE(memory_vm_executor::execute(shift_overflow_program, fx.space, fx.reg_ctx, stack, work_ram,
                                           microfmt::stdout_sink())
                   .success);
}

TEST(MicroVm, UnaryAndSignExtendOperations) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  instruction negate_program[] = {{opcode::push_literal, 5}, {opcode::negate, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(negate_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      static_cast<uint64_t>(-5));

  instruction not_program[] = {{opcode::push_literal, 0}, {opcode::bitwise_not, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(not_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      ~static_cast<uint64_t>(0));

  instruction sext8_program[] = {{opcode::push_literal, 0xFF}, {opcode::sign_extend_8, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(sext8_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      static_cast<uint64_t>(-1));

  instruction sext16_program[] = {{opcode::push_literal, 0x8000}, {opcode::sign_extend_16, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(sext16_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(0x8000))));

  instruction sext32_program[] = {{opcode::push_literal, 0x80000000}, {opcode::sign_extend_32, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(sext32_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(0x80000000))));
}

TEST(MicroVm, ComparisonOperations) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  instruction equal_program[] = {
      {opcode::push_literal, 5}, {opcode::push_literal, 5}, {opcode::equal, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(equal_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      1U);

  instruction not_equal_program[] = {
      {opcode::push_literal, 5}, {opcode::push_literal, 6}, {opcode::not_equal, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(not_equal_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink())
          .value,
      1U);

  instruction less_program[] = {
      {opcode::push_literal, 3}, {opcode::push_literal, 5}, {opcode::less_than, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(less_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      1U);

  instruction greater_program[] = {
      {opcode::push_literal, 5}, {opcode::push_literal, 3}, {opcode::greater_than, 0}, {opcode::halt, 0}};
  EXPECT_EQ(memory_vm_executor::execute(greater_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink())
                .value,
            1U);

  instruction less_or_equal_program[] = {
      {opcode::push_literal, 5}, {opcode::push_literal, 5}, {opcode::less_or_equal, 0}, {opcode::halt, 0}};
  EXPECT_EQ(memory_vm_executor::execute(less_or_equal_program, fx.space, fx.reg_ctx, stack, work_ram,
                                        microfmt::stdout_sink())
                .value,
            1U);

  instruction greater_or_equal_program[] = {
      {opcode::push_literal, 3}, {opcode::push_literal, 5}, {opcode::greater_or_equal, 0}, {opcode::halt, 0}};
  EXPECT_EQ(memory_vm_executor::execute(greater_or_equal_program, fx.space, fx.reg_ctx, stack, work_ram,
                                        microfmt::stdout_sink())
                .value,
            0U);
}

TEST(MicroVm, PrintOpcodesWriteToOutputSink) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[8]{"hello!"};

  std::string out;
  auto container_sink = microfmt::make_container_sink(out);

  instruction program[] = {
      {opcode::push_literal, 42},
      {opcode::print_int, 0},
      {opcode::push_literal, static_cast<uint64_t>(-1)},
      {opcode::print_int, 0}, // Signed decimal: -1
      {opcode::push_literal, 0xFF},
      {opcode::print_hex, 0},
      {opcode::push_literal, static_cast<uint64_t>('!')},
      {opcode::print_char, 0},
      {opcode::push_literal, 0}, // work_ram offset
      {opcode::push_literal, 5}, // length
      {opcode::work_ram_print, 0},
      {opcode::halt, 0}};

  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, container_sink.as_sink());
  ASSERT_TRUE(result.success);
  EXPECT_EQ(out, "42-10xff!hello");
}

TEST(MicroVm, WorkRamPrintOutOfBoundsFails) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  instruction program[] = {{opcode::push_literal, 0}, {opcode::push_literal, 99}, {opcode::work_ram_print, 0},
                          {opcode::halt, 0}};
  EXPECT_FALSE(
      memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).success);
}

TEST(MicroVm, StackManipulationOperations) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  // dup: [7] -> [7, 7] -> add -> 14
  instruction dup_program[] = {{opcode::push_literal, 7}, {opcode::dup, 0}, {opcode::add, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(dup_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      14U);

  // swap: [1, 2] -> [2, 1] -> sub pops a=1, b=2 -> push(b - a) = 1
  instruction swap_program[] = {
      {opcode::push_literal, 1}, {opcode::push_literal, 2}, {opcode::swap, 0}, {opcode::sub, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(swap_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      1U);

  // over: [1, 2] -> [1, 2, 1]
  instruction over_program[] = {
      {opcode::push_literal, 1}, {opcode::push_literal, 2}, {opcode::over, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(over_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      1U);

  // drop: [1, 2] -> [1]
  instruction drop_program[] = {
      {opcode::push_literal, 1}, {opcode::push_literal, 2}, {opcode::drop, 0}, {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(drop_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).value,
      1U);
}

TEST(MicroVm, BranchesRedirectExecution) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  // branch_zero jumps to index 3 (push_literal 99) when the popped value is 0.
  instruction branch_zero_program[] = {{opcode::push_literal, 0},
                                       {opcode::branch_zero, 3},
                                       {opcode::push_literal, 1}, // Skipped
                                       {opcode::push_literal, 99},
                                       {opcode::halt, 0}};
  EXPECT_EQ(
      memory_vm_executor::execute(branch_zero_program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink())
          .value,
      99U);

  // branch_not_zero jumps to index 3 when the popped value is non-zero.
  instruction branch_not_zero_program[] = {{opcode::push_literal, 1},
                                           {opcode::branch_not_zero, 3},
                                           {opcode::push_literal, 1}, // Skipped
                                           {opcode::push_literal, 99},
                                           {opcode::halt, 0}};
  EXPECT_EQ(memory_vm_executor::execute(branch_not_zero_program, fx.space, fx.reg_ctx, stack, work_ram,
                                        microfmt::stdout_sink())
                .value,
            99U);
}

TEST(MicroVm, RegisterLoadAndStoreRoundTrip) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  // Store 123 into register 2, then load it back.
  instruction program[] = {
      {opcode::push_literal, 123}, {opcode::store_register, 2}, {opcode::load_register, 2}, {opcode::halt, 0}};
  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(result.success);
  EXPECT_EQ(result.value, 123U);
  EXPECT_EQ(fx.regs[2], 123U);
}

TEST(MicroVm, RegisterAccessOutOfRangeFails) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  instruction program[] = {{opcode::load_register, 99}, {opcode::halt, 0}};
  EXPECT_FALSE(
      memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).success);
}

TEST(MicroVm, MemoryReadAndWriteRoundTrip) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t vm_work_ram[4]{};

  uint32_t target = 0;
  const auto addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&target));

  instruction program[] = {{opcode::push_literal, addr}, {opcode::push_literal, 0xdeadbeef},
                           {opcode::store_u32, 0},       {opcode::push_literal, addr},
                           {opcode::read_u32, 0},        {opcode::halt, 0}};
  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, vm_work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(result.success);
  EXPECT_EQ(result.value, 0xdeadbeefU);
  EXPECT_EQ(target, 0xdeadbeefU);
}

TEST(MicroVm, MemoryReadNullAddressFails) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[4]{};

  instruction program[] = {{opcode::push_literal, 0}, {opcode::read_u8, 0}, {opcode::halt, 0}};
  EXPECT_FALSE(
      memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).success);
}

TEST(MicroVm, WorkRamReadAndWriteRoundTrip) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[8]{};

  // Store 0xcafebabe at work_ram offset 2, then read it back.
  instruction program[] = {{opcode::push_literal, 2},       {opcode::push_literal, 0xcafebabe},
                           {opcode::work_ram_write_u32, 0}, {opcode::push_literal, 2},
                           {opcode::work_ram_read_u32, 0},  {opcode::halt, 0}};
  auto result = memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink());
  ASSERT_TRUE(result.success);
  EXPECT_EQ(result.value, 0xcafebabeU);
}

TEST(MicroVm, WorkRamOutOfBoundsFails) {
  micro_vm_fixture fx;
  uint64_t stack[4]{};
  uint8_t work_ram[2]{}; // Too small for a uint32_t at offset 0.

  instruction program[] = {{opcode::push_literal, 0}, {opcode::work_ram_read_u32, 0}, {opcode::halt, 0}};
  EXPECT_FALSE(
      memory_vm_executor::execute(program, fx.space, fx.reg_ctx, stack, work_ram, microfmt::stdout_sink()).success);
}

} // namespace
