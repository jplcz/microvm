// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <microfmt/microfmt.hpp>
#include <microfmt/inspector/gdb_registers.hpp>
#include <microvm/variable_context.hpp>
#include <microvm/vm_code_gen.hpp>
#include <microvm/vm_label_allocator.hpp>
#include <microvm/vm_lexer.hpp>

namespace microfmt::inspector {

struct target_arch_traits {
  bool (*lookup_register)(string_view name, uint32_t &out_dwarf_index) noexcept;

  template <typename ArchTag> [[nodiscard]] static constexpr target_arch_traits create() noexcept {
    return target_arch_traits{[](string_view name, uint32_t &out_dwarf_index) noexcept -> bool {
      using traits = gdb::register_traits<ArchTag>;
      if (auto *reg = traits::find_by_name(name)) {
        out_dwarf_index = reg->dwarf_index;
        return true;
      }
      return false;
    }};
  }
};

class vm_compiler {
public:
  struct compile_result {
    bool success;
  };

  /**
   * @brief Compiles scripts using target architecture traits and streams diagnostics to a sink.
   * @param source Script text source string_view.
   * @param gen Code generator bytecode output buffer.
   * @param vars Variable context for work_ram slots.
   * @param arch Target architecture register traits.
   * @param error_sink Sink for streaming compilation errors.
   */
  static compile_result compile(string_view source, vm_code_generator &gen, variable_context &vars,
                                target_arch_traits arch, label_allocator &labels, sink error_sink) noexcept {
    vm_lexer lexer(source);
    vm_compiler compiler(lexer, gen, vars, arch, labels, error_sink);
    return compiler.parse_program();
  }

private:
  constexpr vm_compiler(vm_lexer &lexer, vm_code_generator &gen, variable_context &vars, target_arch_traits arch,
                        label_allocator &labels, sink error_sink) noexcept
      : lexer_(lexer), gen_(gen), vars_(vars), arch_(arch), labels_(labels), error_sink_(error_sink) {}

  vm_lexer &lexer_;
  vm_code_generator &gen_;
  variable_context &vars_;
  target_arch_traits arch_;
  label_allocator &labels_;
  sink error_sink_;

  template <typename... Args> void report_error(string_view format_str, Args &&...args) noexcept {
    const auto &tok = lexer_.current();
    format_to(error_sink_, "[COMPILER ERROR] Line {}, Col {}: ", tok.line, tok.column);
    format_to(error_sink_, format_str, std::forward<Args>(args)...);
    format_to(error_sink_, "\n");
  }

  compile_result parse_program() noexcept {
    while (lexer_.current().type != token_type::end) {
      if (!parse_statement()) {
        // If an error occurred, parse_statement will have already reported it.
        return {false};
      }
      if (gen_.has_overflowed()) {
        report_error("Code generation error: Instruction buffer capacity exceeded (overflow)");
        return {false};
      }
    }
    gen_.halt();

    if (gen_.has_overflowed()) {
      report_error("Code generation error: Instruction buffer capacity exceeded (overflow)");
      return {false};
    }

    if (!labels_.resolve_patches(gen_.program())) {
      report_error("Semantic error: Use of undefined label or patch table overflow");
      return {false};
    }

    return {true};
  }

  bool parse_statement() noexcept {
    const auto &tok = lexer_.current();

    // 1. Code blocks: { stmt1; stmt2; }
    if (tok.type == token_type::lbrace) {
      lexer_.advance();
      while (lexer_.current().type != token_type::rbrace && lexer_.current().type != token_type::end) {
        if (!parse_statement())
          return false;
      }
      if (lexer_.current().type != token_type::rbrace) {
        report_error("Expected '}' to close code block");
        return false;
      }
      lexer_.advance();
      return true;
    }

    // 2. While loops: while (condition) { body }
    if (tok.type == token_type::identifier && tok.text == "while") {
      lexer_.advance();
      if (lexer_.current().type != token_type::lparen) {
        report_error("Expected '(' after 'while'");
        return false;
      }
      lexer_.advance();

      size_t lbl_cond = labels_.create_label();
      size_t lbl_exit = labels_.create_label();

      labels_.define_label(lbl_cond, gen_.size());

      if (!parse_expr()) {
        report_error("Expected expression inside 'while' condition");
        return false;
      }

      if (lexer_.current().type != token_type::rparen) {
        report_error("Expected ')' to close 'while' condition, but found '{}'", lexer_.current().text);
        return false;
      }
      lexer_.advance();

      size_t jump_if_false_idx = gen_.size();
      gen_.branch_zero(0);
      labels_.record_patch(jump_if_false_idx, lbl_exit);

      if (!parse_statement())
        return false;

      size_t jump_back_idx = gen_.size();
      gen_.jump(0);
      labels_.record_patch(jump_back_idx, lbl_cond);

      labels_.define_label(lbl_exit, gen_.size());
      return true;
    }

    // 3. Halt statement
    if (tok.type == token_type::identifier && tok.text == "halt") {
      lexer_.advance();
      if (lexer_.current().type != token_type::semicolon) {
        report_error("Expected ';' after 'halt', but found '{}'", lexer_.current().text);
        return false;
      }
      lexer_.advance();
      gen_.halt();
      return true;
    }

    // 4. Variable declaration: let var_name = expr;
    if (tok.type == token_type::identifier && tok.text == "let") {
      lexer_.advance();
      if (lexer_.current().type != token_type::identifier) {
        report_error("Expected variable identifier after 'let'");
        return false;
      }
      string_view var_name = lexer_.current().text;

      size_t ram_offset = 0;
      if (!vars_.get_or_allocate(var_name, ram_offset)) {
        report_error("Work RAM or symbol table overflow for variable '{}'", var_name);
        return false;
      }

      lexer_.advance();
      if (lexer_.current().type != token_type::assign) {
        report_error("Expected '=' in variable declaration");
        return false;
      }
      lexer_.advance();

      gen_.push(ram_offset);
      if (!parse_expr()) {
        report_error("Invalid expression in variable initialization for '{}'", var_name);
        return false;
      }
      if (lexer_.current().type != token_type::semicolon) {
        report_error("Expected ';' at end of variable declaration");
        return false;
      }
      lexer_.advance();

      gen_.work_ram_write_u64();
      return true;
    }

    // 5. Identifier-led statements (Assignments, print functions)
    if (tok.type == token_type::identifier) {
      string_view id = tok.text;

      // Handle statement-level print functions: print_int(expr); / print_hex(expr); / print_char(expr);
      if (id == "print_int" || id == "print_hex" || id == "print_char") {
        lexer_.advance();
        if (lexer_.current().type != token_type::lparen) {
          report_error("Expected '(' after '{}'", id);
          return false;
        }
        lexer_.advance();

        if (!parse_expr())
          return false;

        if (lexer_.current().type != token_type::rparen) {
          report_error("Expected ')' to close arguments for '{}'", id);
          return false;
        }
        lexer_.advance();

        if (lexer_.current().type != token_type::semicolon) {
          report_error("Expected ';' after print statement");
          return false;
        }
        lexer_.advance();

        if (id == "print_hex")
          gen_.print_hex();
        else if (id == "print_char")
          gen_.print_char();
        else
          gen_.print_int();
        return true;
      }

      // Check if identifier is a target register assignment: reg = expr;
      uint32_t dwarf_idx = 0;
      if (arch_.lookup_register && arch_.lookup_register(id, dwarf_idx)) {
        lexer_.advance();
        if (lexer_.current().type != token_type::assign) {
          report_error("Expected '=' after register '{}'", id);
          return false;
        }
        lexer_.advance();
        if (!parse_expr())
          return false;
        if (lexer_.current().type != token_type::semicolon) {
          report_error("Expected ';' after register assignment");
          return false;
        }
        lexer_.advance();

        gen_.store_reg(dwarf_idx);
        return true;
      }

      // Otherwise, existing variable assignment: var_name = expr;
      size_t ram_offset = 0;
      if (!vars_.get_or_allocate(id, ram_offset)) {
        report_error("Undefined variable '{}'", id);
        return false;
      }

      lexer_.advance();
      if (lexer_.current().type != token_type::assign) {
        report_error("Expected '=' after variable assignment target '{}'", id);
        return false;
      }
      lexer_.advance();

      gen_.push(ram_offset); // LHS target RAM offset
      if (!parse_expr())
        return false; // RHS value expression

      if (lexer_.current().type != token_type::semicolon) {
        report_error("Expected ';' at end of assignment");
        return false;
      }
      lexer_.advance();

      gen_.work_ram_write_u64();
      return true;
    }

    report_error("Unexpected token '{}' at statement start", tok.text);
    return false;
  }

  bool parse_expr() noexcept { return parse_comparison(); }

  // Level 0: Comparisons (<, >, ==, !=, <=, >=)
  bool parse_comparison() noexcept {
    if (!parse_bit_or())
      return false;

    while (lexer_.current().type == token_type::less || lexer_.current().type == token_type::greater ||
           lexer_.current().type == token_type::less_equal || lexer_.current().type == token_type::greater_equal ||
           lexer_.current().type == token_type::equal_equal || lexer_.current().type == token_type::not_equal) {

      token_type op = lexer_.current().type;
      lexer_.advance();
      if (!parse_bit_or()) {
        report_error("Expected expression after comparison operator");
        return false;
      }

      if (op == token_type::less)
        gen_.less_than();
      else if (op == token_type::greater)
        gen_.greater_than();
      else if (op == token_type::less_equal)
        gen_.less_or_equal();
      else if (op == token_type::greater_equal)
        gen_.greater_or_equal();
      else if (op == token_type::equal_equal)
        gen_.equal();
      else if (op == token_type::not_equal)
        gen_.not_equal();
    }
    return true;
  }

  // Level 1: Bitwise OR (|)
  bool parse_bit_or() noexcept {
    if (!parse_bit_xor())
      return false;
    while (lexer_.current().type == token_type::pipe) {
      lexer_.advance();
      if (!parse_bit_xor()) {
        report_error("Expected expression after '|'");
        return false;
      }
      gen_.bit_or();
    }
    return true;
  }

  // Level 2: Bitwise XOR (^)
  bool parse_bit_xor() noexcept {
    if (!parse_bit_and())
      return false;
    while (lexer_.current().type == token_type::caret) {
      lexer_.advance();
      if (!parse_bit_and()) {
        report_error("Expected expression after '^'");
        return false;
      }
      gen_.bit_xor();
    }
    return true;
  }

  // Level 3: Bitwise AND (&)
  bool parse_bit_and() noexcept {
    if (!parse_shift())
      return false;
    while (lexer_.current().type == token_type::ampersand) {
      lexer_.advance();
      if (!parse_shift()) {
        report_error("Expected expression after '&'");
        return false;
      }
      gen_.bit_and();
    }
    return true;
  }

  // Level 4: Shifts (<<, >>)
  bool parse_shift() noexcept {
    if (!parse_additive())
      return false;
    while (lexer_.current().type == token_type::shl_op || lexer_.current().type == token_type::shr_op) {
      token_type op = lexer_.current().type;
      lexer_.advance();
      if (!parse_additive()) {
        report_error("Expected expression after shift operator");
        return false;
      }
      if (op == token_type::shl_op)
        gen_.shl();
      else
        gen_.shr();
    }
    return true;
  }

  // Level 5: Additive (+, -)
  bool parse_additive() noexcept {
    if (!parse_multiplicative())
      return false;
    while (lexer_.current().type == token_type::plus || lexer_.current().type == token_type::minus) {
      token_type op = lexer_.current().type;
      lexer_.advance();
      if (!parse_multiplicative()) {
        report_error("Expected expression after additive operator");
        return false;
      }
      if (op == token_type::plus)
        gen_.add();
      else
        gen_.sub();
    }
    return true;
  }

  // Level 6: Multiplicative (*, /, %)
  bool parse_multiplicative() noexcept {
    if (!parse_operand())
      return false;
    while (lexer_.current().type == token_type::star || lexer_.current().type == token_type::slash ||
           lexer_.current().type == token_type::percent) {
      token_type op = lexer_.current().type;
      lexer_.advance();
      if (!parse_operand()) {
        report_error("Expected operand after multiplicative operator");
        return false;
      }
      if (op == token_type::star)
        gen_.mul();
      else if (op == token_type::slash)
        gen_.div();
      else
        gen_.mod();
    }
    return true;
  }

  bool parse_operand() noexcept {
    const auto &tok = lexer_.current();

    // Parenthesized grouping: ( expr )
    if (tok.type == token_type::lparen) {
      lexer_.advance(); // consume '('

      // Parse the inner expression using the lowest precedence level (parse_expr)
      if (!parse_expr()) {
        report_error("Invalid expression inside parentheses");
        return false;
      }

      if (lexer_.current().type != token_type::rparen) {
        report_error("Expected ')' to close parenthesized expression, but found '{}'", lexer_.current().text);
        return false;
      }
      lexer_.advance(); // consume ')'
      return true;
    }

    // Number literals
    if (tok.type == token_type::number) {
      gen_.push(tok.num_val);
      lexer_.advance();
      return true;
    }

    // Identifiers (Registers, Variables, or Pseudo-Functions)
    if (tok.type == token_type::identifier) {
      string_view id = tok.text;

      // Handle pseudo-function reads within expressions: read_u64(...)
      if (id == "read_u8" || id == "read_u16" || id == "read_u32" || id == "read_u64") {
        string_view func_name = id;
        lexer_.advance(); // consume function name

        if (lexer_.current().type != token_type::lparen) {
          report_error("Expected '(' after pseudo-function '{}'", func_name);
          return false;
        }
        lexer_.advance(); // consume '('

        if (!parse_expr()) {
          report_error("Invalid argument expression for pseudo-function '{}'", func_name);
          return false;
        }

        if (lexer_.current().type != token_type::rparen) {
          report_error("Expected ')' to close arguments for pseudo-function '{}', but found '{}'", func_name,
                       lexer_.current().text);
          return false;
        }
        lexer_.advance(); // consume ')'

        if (func_name == "read_u8")
          gen_.read_u8();
        else if (func_name == "read_u16")
          gen_.read_u16();
        else if (func_name == "read_u32")
          gen_.read_u32();
        else if (func_name == "read_u64")
          gen_.read_u64();

        return true;
      }

      // Check target registers
      uint32_t dwarf_idx = 0;
      if (arch_.lookup_register && arch_.lookup_register(id, dwarf_idx)) {
        gen_.load_reg(dwarf_idx);
        lexer_.advance();
        return true;
      }

      // Fallback: Work RAM variable load
      size_t ram_offset = 0;
      if (!vars_.get_or_allocate(id, ram_offset)) {
        report_error("Undefined variable or symbol table overflow for identifier '{}'", id);
        return false;
      }
      gen_.push(ram_offset);
      gen_.work_ram_read_u64();
      lexer_.advance();
      return true;
    }

    report_error("Expected number, register, variable, or parenthesized expression, but found '{}'", tok.text);
    return false;
  }
};

} // namespace microfmt::inspector