// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <microfmt/microfmt.hpp>

namespace microfmt::inspector {

enum class token_type {
  end,
  identifier,
  number,
  string_literal,
  assign,        // =
  plus,          // +
  minus,         // -
  star,          // *
  slash,         // /
  percent,       // %
  ampersand,     // &
  pipe,          // |
  caret,         // ^
  shl_op,        // <<
  shr_op,        // >>
  equal_equal,   // ==
  not_equal,     // !=
  less,          // <
  less_equal,    // <=
  greater,       // >
  greater_equal, // >=
  lparen,        // (
  rparen,        // )
  lbrace,        // {
  rbrace,        // }
  comma,         // ,
  semicolon      // ;
};

struct token {
  token_type type;
  string_view text;
  uint64_t num_val;
  size_t line;   // New: 1-based line number
  size_t column; // New: 1-based column number
};

class MICROFMT_POINTER vm_lexer {
public:
  constexpr explicit vm_lexer(string_view source MICROFMT_LIFETIMEBOUND MICROFMT_LIFETIME_CAPTURE_BY_THIS) noexcept
      : sv_(source) {
    advance();
  }

  [[nodiscard]] constexpr const token &current() const & noexcept MICROFMT_LIFETIMEBOUND { return current_; }

  constexpr void advance() noexcept {
    // Skip whitespace and newlines
    while (!sv_.empty()) {
      char c = sv_.front();
      if (c == '\n') {
        line_++;
        column_ = 1;
        sv_.remove_prefix(1);
      } else if (c == ' ' || c == '\t' || c == '\r') {
        column_++;
        sv_.remove_prefix(1);
      } else {
        break;
      }
    }

    if (sv_.empty()) {
      current_ = {token_type::end, {}, 0, line_, column_};
      return;
    }

    char c = sv_.front();

    // Two-character operators
    if (sv_.size() >= 2) {
      if (c == '=' && sv_[1] == '=') {
        sv_.remove_prefix(2);
        current_ = {token_type::equal_equal, "==", 0, line_, column_};
        return;
      }
      if (c == '!' && sv_[1] == '=') {
        sv_.remove_prefix(2);
        current_ = {token_type::not_equal, "!=", 0, line_, column_};
        return;
      }
      if (c == '<' && sv_[1] == '=') {
        sv_.remove_prefix(2);
        current_ = {token_type::less_equal, "<=", 0, line_, column_};
        return;
      }
      if (c == '>' && sv_[1] == '=') {
        sv_.remove_prefix(2);
        current_ = {token_type::greater_equal, ">=", 0, line_, column_};
        return;
      }
      if (c == '<' && sv_[1] == '<') {
        sv_.remove_prefix(2);
        current_ = {token_type::shl_op, "<<", 0, line_, column_};
        return;
      }
      if (c == '>' && sv_[1] == '>') {
        sv_.remove_prefix(2);
        current_ = {token_type::shr_op, ">>", 0, line_, column_};
        return;
      }
    }

    // Single-character punctuation
    if (c == '<') {
      sv_.remove_prefix(1);
      current_ = {token_type::less, "<", 0, line_, column_};
      return;
    }
    if (c == '>') {
      sv_.remove_prefix(1);
      current_ = {token_type::greater, ">", 0, line_, column_};
      return;
    }
    if (c == '{') {
      sv_.remove_prefix(1);
      current_ = {token_type::lbrace, "{", 0, line_, column_};
      return;
    }
    if (c == '}') {
      sv_.remove_prefix(1);
      current_ = {token_type::rbrace, "}", 0, line_, column_};
      return;
    }
    if (c == '=') {
      sv_.remove_prefix(1);
      current_ = {token_type::assign, "=", 0, line_, column_};
      return;
    }
    if (c == '+') {
      sv_.remove_prefix(1);
      current_ = {token_type::plus, "+", 0, line_, column_};
      return;
    }
    if (c == '-') {
      sv_.remove_prefix(1);
      current_ = {token_type::minus, "-", 0, line_, column_};
      return;
    }
    if (c == '*') {
      sv_.remove_prefix(1);
      current_ = {token_type::star, "*", 0, line_, column_};
      return;
    }
    if (c == '/') {
      sv_.remove_prefix(1);
      current_ = {token_type::slash, "/", 0, line_, column_};
      return;
    }
    if (c == '%') {
      sv_.remove_prefix(1);
      current_ = {token_type::percent, "%", 0, line_, column_};
      return;
    }
    if (c == '&') {
      sv_.remove_prefix(1);
      current_ = {token_type::ampersand, "&", 0, line_, column_};
      return;
    }
    if (c == '|') {
      sv_.remove_prefix(1);
      current_ = {token_type::pipe, "|", 0, line_, column_};
      return;
    }
    if (c == '^') {
      sv_.remove_prefix(1);
      current_ = {token_type::caret, "^", 0, line_, column_};
      return;
    }
    if (c == ';') {
      sv_.remove_prefix(1);
      current_ = {token_type::semicolon, ";", 0, line_, column_};
      return;
    }
    if (c == '(') {
      sv_.remove_prefix(1);
      current_ = {token_type::lparen, "(", 0, line_, column_};
      return;
    }
    if (c == ')') {
      sv_.remove_prefix(1);
      current_ = {token_type::rparen, ")", 0, line_, column_};
      return;
    }
    if (c == ',') {
      sv_.remove_prefix(1);
      current_ = {token_type::comma, ",", 0, line_, column_};
      return;
    }

    // String literals
    if (c == '"') {
      const char *start = sv_.data(); // points after opening quote
      size_t len = 0;
      bool closed = false;

      while (!sv_.empty()) {
        char nc = sv_.front();
        if (nc == '\n') {
          line_++;
          column_ = 1;
        }
        sv_.remove_prefix(1);
        column_++;

        if (nc == '"') {
          closed = true;
          break;
        }
        len++;
      }

      if (!closed) {
      }

      // string_view content excludes the surrounding quotes
      string_view str_val(start, len);
      current_ = {token_type::string_literal, str_val, 0, line_, column_};
      return;
    }

    // Number literals (decimal or hex)
    if (c >= '0' && c <= '9') {
      uint64_t val = 0;
      bool is_hex = false;
      const char *start = sv_.data();

      if (c == '0' && sv_.size() > 1 && (sv_[1] == 'x' || sv_[1] == 'X')) {
        is_hex = true;
        sv_.remove_prefix(2);
      }

      while (!sv_.empty()) {
        char nc = sv_.front();
        uint64_t digit = 16;
        if (nc >= '0' && nc <= '9')
          digit = (uint64_t)(nc - '0');
        else if (is_hex && nc >= 'a' && nc <= 'f')
          digit = (uint64_t)(nc - 'a' + 10);
        else if (is_hex && nc >= 'A' && nc <= 'F')
          digit = (uint64_t)(nc - 'A' + 10);
        else
          break;

        val = val * (is_hex ? 16 : 10) + digit;
        sv_.remove_prefix(1);
      }
      current_ = {token_type::number, {start, static_cast<size_t>(sv_.data() - start)}, val, line_, column_};
      return;
    }

    // Identifiers and Keywords
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
      const char *start = sv_.data();
      size_t len = 0;
      while (!sv_.empty() &&
             ((sv_.front() >= 'a' && sv_.front() <= 'z') || (sv_.front() >= 'A' && sv_.front() <= 'Z') ||
              (sv_.front() >= '0' && sv_.front() <= '9') || sv_.front() == '_')) {
        len++;
        sv_.remove_prefix(1);
      }
      current_ = {token_type::identifier, {start, len}, 0, line_, column_};
      return;
    }

    // Unrecognized character
    sv_.remove_prefix(1);
    current_ = {token_type::end, {}, 0, line_, column_};
  }

private:
  string_view sv_;
  token current_{token_type::end, {}, 0, 0, 0};
  size_t line_{1};
  size_t column_{1};
};

} // namespace microfmt::inspector