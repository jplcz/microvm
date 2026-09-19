// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include <microvm/variable_context.hpp>

#include <gtest/gtest.h>

namespace {

using microfmt::span;
using microfmt::inspector::variable_context;
using microfmt::inspector::variable_symbol;

TEST(VariableContext, AllocatesNewVariableWithDefaultSize) {
  variable_symbol symbols[4]{};
  variable_context ctx(symbols);

  size_t offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("x", offset));
  EXPECT_EQ(offset, 0U);
}

TEST(VariableContext, RepeatedLookupReturnsSameOffset) {
  variable_symbol symbols[4]{};
  variable_context ctx(symbols);

  size_t first_offset = SIZE_MAX;
  size_t second_offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("x", first_offset));
  ASSERT_TRUE(ctx.get_or_allocate("x", second_offset));
  EXPECT_EQ(first_offset, second_offset);
}

TEST(VariableContext, DistinctNamesGetDistinctOffsets) {
  variable_symbol symbols[4]{};
  variable_context ctx(symbols);

  size_t x_offset = SIZE_MAX;
  size_t y_offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("x", x_offset));
  ASSERT_TRUE(ctx.get_or_allocate("y", y_offset));
  EXPECT_NE(x_offset, y_offset);
}

TEST(VariableContext, AllocationsAreEightByteAligned) {
  variable_symbol symbols[4]{};
  variable_context ctx(symbols);

  size_t a_offset = SIZE_MAX;
  size_t b_offset = SIZE_MAX;
  size_t c_offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("a", a_offset, 1));
  ASSERT_TRUE(ctx.get_or_allocate("b", b_offset, 3));
  ASSERT_TRUE(ctx.get_or_allocate("c", c_offset));

  EXPECT_EQ(a_offset, 0U);
  EXPECT_EQ(b_offset, 8U); // Aligned up from a's 1-byte allocation.
  EXPECT_EQ(c_offset, 16U); // Aligned up from b's 3-byte allocation.
}

TEST(VariableContext, FailsWhenSymbolTableIsFull) {
  variable_symbol symbols[2]{};
  variable_context ctx(symbols);

  size_t offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("x", offset));
  ASSERT_TRUE(ctx.get_or_allocate("y", offset));
  EXPECT_FALSE(ctx.get_or_allocate("z", offset)); // Table only holds 2 symbols.
}

TEST(VariableContext, FailsWhenWorkRamBudgetIsExhausted) {
  variable_symbol symbols[4]{};
  variable_context ctx(symbols);

  size_t offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("huge", offset, 1024));
  EXPECT_FALSE(ctx.get_or_allocate("other", offset, 8)); // Exceeds the 1024-byte work_ram guard.
}

TEST(VariableContext, ResetClearsAllocationsAndOffsets) {
  variable_symbol symbols[4]{};
  variable_context ctx(symbols);

  size_t offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("x", offset));
  ctx.reset();

  size_t new_offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("y", new_offset));
  EXPECT_EQ(new_offset, 0U); // Offsets restart from zero after reset().

  // The symbol table itself was overwritten in place, so the old "x" entry no
  // longer resolves as a hit; it now aliases the newly allocated "y" slot.
  size_t x_offset = SIZE_MAX;
  ASSERT_TRUE(ctx.get_or_allocate("x", x_offset));
  EXPECT_EQ(x_offset, 8U);
}

TEST(VariableContext, ConstexprConstructionAndAllocationAreEvaluableAtCompileTime) {
  constexpr bool ok = [] {
    variable_symbol symbols[2]{};
    variable_context ctx{span<variable_symbol>(symbols)};
    size_t offset = 0;
    return ctx.get_or_allocate("x", offset) && offset == 0;
  }();
  static_assert(ok, "variable_context must support constexpr allocation");
  EXPECT_TRUE(ok);
}

} // namespace
