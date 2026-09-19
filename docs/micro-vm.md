<!--
SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>

SPDX-License-Identifier: BSD-2-Clause
-->

# Micro VM: bytecode scripting for diagnostics

`micro_vm.hpp` and `vm_code_gen.hpp` implement a tiny, zero-allocation stack
machine for scripted diagnostics: expression evaluation over CPU registers
and target memory, conditional decoding logic, and formatted output, all
without dynamic allocation, exceptions, or RTTI. It is intended for embedding
scripted checks into crash handlers, remote debug protocols, and inspection
tools where the script, its scratch storage, and its execution bounds are all
supplied and owned by the caller.

The VM is deliberately small: a single 64-bit evaluation stack, a
caller-owned scratch RAM region, register access through
`register_context_ref`, and fault-safe memory access through
`address_space_ref`. Every operation that can fail (stack underflow/overflow,
out-of-range register or work-RAM access, a null target address, division by
zero, an out-of-range shift, or an out-of-bounds jump) aborts execution and
reports failure instead of invoking undefined behavior.

## Instruction encoding

```cpp
struct instruction {
  opcode op;
  uint64_t operand;
};
```

A program is a `span<const instruction>`. Most opcodes ignore `operand`;
`push_literal` uses it as a 64-bit constant, `load_register`/`store_register`
use it as a register index, and `jump`/`branch_zero`/`branch_not_zero` use it
as an absolute instruction index within the same program.

All stack values are `uint64_t`. Arithmetic, comparisons, and bitwise
operators treat the stack as unsigned; `negate` and `sign_extend_*`
reinterpret the popped value as signed where documented. For any binary
operator `OP`, pushing `x` then `y` and executing `OP` computes `x OP y`
(the executor's internal `pop()` order is the reverse of push order, but the
generator and this document always describe operations in left-to-right
source order).

## Opcode reference

### Literals and registers

| Opcode | Effect |
|---|---|
| `push_literal` | Pushes the 64-bit `operand` constant. |
| `load_register` | Reads register `operand` through `register_context_ref::read<uint64_t>` and pushes it. |
| `store_register` | Pops a value and writes it to register `operand` through `register_context_ref::write<uint64_t>`. |

Registers are always accessed as `uint64_t`; a `register_context_ref` backed
by narrower fields (for example `uint32_t`) must be adapted (widened) before
being exposed to the VM, since register width must match the read/write call
exactly (see [Architectures and registers](architectures-and-registers.md)).

### Fault-safe target memory

| Opcode | Effect |
|---|---|
| `read_u8`/`read_u16`/`read_u32`/`read_u64` | Pops an address, reads the target width through `address_space_ref::read`, zero-extends, and pushes it. |
| `store_u8`/`store_u16`/`store_u32`/`store_u64` | Pops a value then an address, and writes the truncated value through `address_space_ref::write`. |

These opcodes fault (abort execution) whenever the underlying
`address_space_ref` read or write fails, including a null address. They never
dereference raw pointers directly; all safety guarantees of the configured
address-space provider apply (see
[Memory and remote objects](memory-and-objects.md)).

### Scratch work RAM

| Opcode | Effect |
|---|---|
| `work_ram_read_u8`/`_u16`/`_u32`/`_u64` | Pops an offset, bounds-checks it against the caller-supplied `work_ram` span, reads the width via `memcpy`, and pushes the zero-extended value. |
| `work_ram_write_u8`/`_u16`/`_u32`/`_u64` | Pops a value then an offset, bounds-checks, and writes the truncated value into `work_ram` via `memcpy`. |

Work RAM is plain caller-owned scratch memory (`span<uint8_t>`), not target
memory; it never goes through the address space. Use it for temporary
buffers, decoded strings, or intermediate structures a script needs to build
before printing or before writing back to target memory.

### Output sink printing

| Opcode | Effect |
|---|---|
| `print_int` | Pops a value and formats it as a signed decimal (`"{}"` on the reinterpreted `int64_t`) to the executor's output sink. |
| `print_hex` | Pops a value and formats it as `0x`-prefixed hexadecimal (`"0x{:x}"`) to the sink. |
| `print_char` | Pops a value and writes its low byte as a single character to the sink. |
| `work_ram_print` | Pops a length then an offset, bounds-checks against `work_ram`, and writes that byte range to the sink as a `string_view`. |

Printing opcodes let a script format its own diagnostic output without the
host needing to inspect the final stack value; combine them with `dup` when
a value must both be printed and used in subsequent computation.

### Arithmetic and division

| Opcode | Effect |
|---|---|
| `add`, `sub`, `mul` | Standard two's-complement 64-bit arithmetic. |
| `div`, `mod` | Fault if the divisor is zero; otherwise unsigned division/remainder. |

### Bitwise and shifts

| Opcode | Effect |
|---|---|
| `bit_and`, `bit_or`, `bit_xor`, `and_not` | Standard bitwise operators (`and_not` computes `x & ~y`). |
| `shl`, `shr` | Fault if the shift count is `>= 64` (avoids undefined-behavior shifts). |

### Unary and sign extension

| Opcode | Effect |
|---|---|
| `negate` | Two's-complement negation of the top-of-stack value. |
| `bitwise_not` | Bitwise complement of the top-of-stack value. |
| `sign_extend_8`/`_16`/`_32` | Reinterprets the low 8/16/32 bits as signed and sign-extends to 64 bits in place. |

### Comparisons

`equal`, `not_equal`, `less_than`, `greater_than`, `less_or_equal`, and
`greater_or_equal` all pop two unsigned values and push `1` or `0`.

### Stack manipulation

| Opcode | Effect |
|---|---|
| `dup` | `[a]` -> `[a, a]` |
| `drop` | `[a]` -> `[]` |
| `swap` | `[a, b]` -> `[b, a]` |
| `over` | `[a, b]` -> `[a, b, a]` |

### Flow control

| Opcode | Effect |
|---|---|
| `jump` | Unconditionally sets the instruction pointer to `operand`. |
| `branch_zero` | Pops a condition; jumps to `operand` if it is zero. |
| `branch_not_zero` | Pops a condition; jumps to `operand` if it is nonzero. |
| `halt` | Stops execution successfully; the result is the current top-of-stack value (or `0` if the stack is empty). |

Jump targets are absolute instruction indices into the same program and are
bounds-checked against the program length; an out-of-range target aborts
execution.

## Executing a program

```cpp
class memory_vm_executor {
public:
  struct result {
    uint64_t value;
    bool success;
  };

  static result execute(span<const instruction> program, address_space_ref space,
                        register_context_ref regs, span<uint64_t> evaluation_stack,
                        span<uint8_t> work_ram, sink output_sink,
                        size_t max_steps = 10000) noexcept;
};
```

All storage is caller-owned: the evaluation stack, work RAM, and the address
space's own scratch (if any) must be sized and kept alive by the caller for
the duration of `execute`. Nothing is allocated by the VM itself.

`result.success` is `false` whenever the program aborts before reaching
`halt` (empty program, empty evaluation stack, stack
underflow/overflow, a fault from any opcode above, an out-of-range jump
target, or the step watchdog). `result.value` is `0` in that case.
`result.success` is `true` on `halt`, or after the last instruction executes
without a `halt` and the stack still holds a value; `result.value` is the
top-of-stack value at that point (or `0` for an empty stack).

`max_steps` bounds the number of instructions executed (default `10000`),
guarding against runaway or maliciously crafted programs (for example an
unconditional self-jump). Exceeding it aborts execution with `success =
false`. Choose a limit appropriate to the largest legitimate script for the
call site; scripts embedded in signal handlers or other tightly bounded
contexts should use a much smaller limit.

## Generating bytecode with `vm_code_generator`

`vm_code_gen.hpp` provides a fluent, `constexpr`-friendly builder that emits
directly into a caller-owned `span<instruction>` buffer, with one method per
opcode (`push`, `load_reg`, `store_reg`, `add`, `less_than`, `jump`, `halt`,
...). It never allocates and silently stops emitting once the buffer is
full, so callers should size buffers generously and check `size()`/
`has_space()` if a program's length is not known in advance:

```cpp
instruction code[16];
vm_code_generator gen(code);
gen.push(5).push(3).add().push(2).mul().print_int().halt();

auto result = memory_vm_executor::execute(gen.program(), space, regs, stack, work_ram, out);
```

`gen.program()` returns the `span<const instruction>` actually emitted
(`buffer.subspan(0, size())`), ready to pass to `execute`. `current_offset()`
returns the index the next instruction will occupy, which is useful when
computing jump targets while assembling branching code; because instruction
addresses are simply their index in the program, forward jump targets can
also be computed by counting instructions if the program's structure is
known ahead of time. See `examples/vm_code_gen_demo.cpp` for a complete
walkthrough, including a `max(a, b)` program built with `branch_zero` and
`jump`.

## Integrating registers and memory

Bind the VM to a concrete register bank and address space the same way any
other inspector API does:

```cpp
using register_bank = std::array<uint64_t, 4>;

bool read_register(const register_bank *state, microfmt::address_space_ref, uint32_t index,
                   void *dest, size_t size) noexcept {
  if (!state || size != sizeof(uint64_t) || index >= state->size())
    return false;
  std::memcpy(dest, &(*state)[index], sizeof(uint64_t));
  return true;
}

bool write_register(register_bank *state, microfmt::address_space_ref, uint32_t index,
                    const void *src, size_t size) noexcept {
  if (!state || size != sizeof(uint64_t) || index >= state->size())
    return false;
  std::memcpy(&(*state)[index], src, sizeof(uint64_t));
  return true;
}

register_bank regs{};
std::byte scratch[8]{};
auto space = microfmt::address_space_ref::make<microfmt::local_space_tag>();
auto reg_ctx = microfmt::make_register_context_ref<read_register, write_register>(regs, space, scratch);
```

`read_register`/`write_register` must match the `size == sizeof(uint64_t)`
contract described above; see
[Architectures and registers](architectures-and-registers.md) and
[Traits, contexts, and type erasure](traits-and-contexts.md) for the general
`register_context_ref`/`address_space_ref` provider model, including how to
back the VM with a real hardware register file or a remote/foreign address
space instead of `local_space_tag`.

## Compiling scripts with `vm_compiler`

`vm_lexer.hpp` and `vm_compiler.hpp` add a small, C-like **compiled scripting
language** on top of the raw bytecode described above. It lowers directly to
`instruction`s through `vm_code_generator`, using a hand-written
recursive-descent parser with no allocation, exceptions, or RTTI, so it can be
compiled in the same constrained contexts the VM itself targets (a signal
handler compiling a script once at startup, an offline tool preparing scripts
for an embedded target, and so on).

```
source text
  -> vm_lexer          (tokenizes into token_type/token, tracks line/column)
  -> vm_compiler        (recursive-descent parser)
  -> vm_code_generator   (emits `instruction`s into a caller-owned buffer)
  -> label_allocator     (resolves forward/backward jump targets)
  -> variable_context    (maps `let`-declared names to work_ram offsets)
```

### Syntax

Statements are terminated by `;` or grouped in `{ ... }` blocks:

| Statement | Effect |
|---|---|
| `let name = expr;` | Declares a variable, backed by an 8-byte `work_ram` slot allocated by `variable_context::get_or_allocate`. |
| `name = expr;` | Reassigns an existing variable, or a target register if `name` resolves through `target_arch_traits::lookup_register` (for example `rax = expr;`). |
| `while (cond) { ... }` | Loop, compiled to `branch_zero`/`jump` around two labels obtained from `label_allocator`. |
| `print_int(expr);` / `print_hex(expr);` / `print_char(expr);` | Statement-level printing (`gen_.print_int()`/`gen_.print_hex()`/`gen_.print_char()`). |
| `halt;` | Emits `halt`. |

Expressions use standard C-style precedence, lowest to highest:

```
comparisons (< > <= >= == !=)
  -> | (bitwise or)
  -> ^ (bitwise xor)
  -> & (bitwise and)
  -> << >> (shifts)
  -> + - (additive)
  -> * / % (multiplicative)
  -> operand
```

An operand is one of:

* an integer literal, decimal or `0x`-prefixed hexadecimal;
* a parenthesized sub-expression `( expr )`;
* a **register name** resolved through `target_arch_traits` (loads via
  `gen_.load_reg(dwarf_index)`, stores via `gen_.store_reg(dwarf_index)`);
* a **variable name** (work-RAM backed, auto-allocated on first use, loaded
  via `gen_.push(offset); gen_.work_ram_read_u64();`);
* a fault-safe memory read pseudo-function call,
  `read_u8(addr)`/`read_u16(addr)`/`read_u32(addr)`/`read_u64(addr)`.

### Example

```c
let i = 0;
while (i < 3) {
  print_int(i);
  i = i + 1;
}

let tcb_ptr = rax;
let target_addr = tcb_ptr + 16;
let raw_flags = read_u64(target_addr);
print_hex(raw_flags & 0xFF00);
halt;
```

### Compiling and running

```cpp
instruction code[1024];
vm_code_generator gen(code);

variable_symbol symbols[8]{};
variable_context vars(symbols);

label_allocator::label_info label_infos[64];
label_allocator::patch_site patch_sites[64];
label_allocator labels(label_infos, patch_sites);

const auto arch = target_arch_traits::create<microfmt::gdb::tags::x86_64>();
const auto result = vm_compiler::compile(script, gen, vars, arch, labels, error_sink);
if (!result.success) {
  // A `[COMPILER ERROR] Line L, Col C: ...` message was already written to error_sink.
  return;
}

const auto program = gen.program(); // ready for memory_vm_executor::execute
```

`target_arch_traits::create<ArchTag>()` binds register-name lookup to a
`gdb::register_traits<ArchTag>` specialization (see
[GDB Remote Serial Protocol](gdb-protocol.md) and
[Architectures and registers](architectures-and-registers.md)), so the same
script source can target different architectures by swapping the `ArchTag`.

`vm_compiler::compile` always appends a trailing `halt` and resolves every
recorded jump patch before returning success; a `false` result means either a
syntax/semantic error (already reported to `error_sink` with a line/column
prefix), a `vm_code_generator` buffer overflow, or an unresolved/overflowed
label table. See `examples/vm_compiler_demo.cpp` for a full compile-dump-execute
walkthrough, including a register-aware script and an opcode disassembly
dump.

## Safety notes

* Every memory- and register-facing opcode goes through the caller-supplied
  `address_space_ref`/`register_context_ref`, so target-memory faults are
  reported as ordinary failures rather than crashing the host process.
* Stack and work-RAM accesses are bounds-checked against the spans the
  caller provided; there is no implicit growth.
* Division, modulo, and shift opcodes explicitly guard against
  divide-by-zero and out-of-range shift counts instead of relying on
  compiler-defined or undefined behavior.
* The step watchdog (`max_steps`) is the only defense against infinite
  loops; callers embedding untrusted or generated scripts should choose a
  conservative limit for their context.
* The VM performs no dynamic allocation and holds no static/global state, so
  it is safe to use from signal handlers and other reentrant or
  interrupt-context callers, provided the supplied sink and address space are
  themselves safe in that context.
