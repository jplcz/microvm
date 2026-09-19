// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include <microvm/vm_lexer.hpp>

#include <gtest/gtest.h>

namespace {

using microfmt::inspector::token_type;
using microfmt::inspector::vm_lexer;

TEST(VmLexer, EmptySourceYieldsEndToken) {
  vm_lexer lexer("");
  EXPECT_EQ(lexer.current().type, token_type::end);
  EXPECT_TRUE(lexer.current().text.empty());
}

TEST(VmLexer, SkipsWhitespaceAndNewlines) {
  vm_lexer lexer("  \t\r\n x0");
  EXPECT_EQ(lexer.current().type, token_type::identifier);
  EXPECT_EQ(lexer.current().text, "x0");
}

TEST(VmLexer, LexesIdentifiers) {
  vm_lexer lexer("print_hex");
  EXPECT_EQ(lexer.current().type, token_type::identifier);
  EXPECT_EQ(lexer.current().text, "print_hex");

  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::end);
}

TEST(VmLexer, LexesDecimalNumber) {
  vm_lexer lexer("42");
  EXPECT_EQ(lexer.current().type, token_type::number);
  EXPECT_EQ(lexer.current().text, "42");
  EXPECT_EQ(lexer.current().num_val, 42U);
}

TEST(VmLexer, LexesHexNumber) {
  vm_lexer lexer("0x10");
  EXPECT_EQ(lexer.current().type, token_type::number);
  EXPECT_EQ(lexer.current().text, "0x10");
  EXPECT_EQ(lexer.current().num_val, 16U);
}

TEST(VmLexer, LexesUppercaseHexNumber) {
  vm_lexer lexer("0XFF");
  EXPECT_EQ(lexer.current().type, token_type::number);
  EXPECT_EQ(lexer.current().num_val, 255U);
}

TEST(VmLexer, LexesZeroLiteralWithoutHexPrefix) {
  vm_lexer lexer("0");
  EXPECT_EQ(lexer.current().type, token_type::number);
  EXPECT_EQ(lexer.current().text, "0");
  EXPECT_EQ(lexer.current().num_val, 0U);
}

TEST(VmLexer, LexesSingleCharacterPunctuation) {
  vm_lexer assign("=");
  EXPECT_EQ(assign.current().type, token_type::assign);
  EXPECT_EQ(assign.current().text, "=");

  vm_lexer plus("+");
  EXPECT_EQ(plus.current().type, token_type::plus);
  EXPECT_EQ(plus.current().text, "+");

  vm_lexer minus("-");
  EXPECT_EQ(minus.current().type, token_type::minus);
  EXPECT_EQ(minus.current().text, "-");

  vm_lexer semi(";");
  EXPECT_EQ(semi.current().type, token_type::semicolon);
  EXPECT_EQ(semi.current().text, ";");
}

TEST(VmLexer, TokenizesFullAssignmentStatement) {
  vm_lexer lexer("x0 = 1 + 0x2;");

  EXPECT_EQ(lexer.current().type, token_type::identifier);
  EXPECT_EQ(lexer.current().text, "x0");

  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::assign);

  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::number);
  EXPECT_EQ(lexer.current().num_val, 1U);

  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::plus);

  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::number);
  EXPECT_EQ(lexer.current().num_val, 2U);
  EXPECT_EQ(lexer.current().text, "0x2");

  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::semicolon);

  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::end);
}

TEST(VmLexer, UnrecognizedCharacterYieldsEndToken) {
  vm_lexer lexer("@");
  EXPECT_EQ(lexer.current().type, token_type::end);
}

TEST(VmLexer, AdvancingPastEndStaysAtEnd) {
  vm_lexer lexer("x");
  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::end);
  lexer.advance();
  EXPECT_EQ(lexer.current().type, token_type::end);
}

TEST(VmLexer, TokenTextPointsIntoOriginalSourceBuffer) {
  // The lexer must not copy text; token views should alias the source buffer.
  const char source[] = "print_hex";
  vm_lexer lexer(source);
  EXPECT_EQ(lexer.current().text.data(), source);
}

} // namespace
