<!--
SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>

SPDX-License-Identifier: BSD-2-Clause
-->

# jplcz_microvm

`jplcz_microvm` is a tiny, zero-allocation bytecode stack machine, code
generator, and compiled C-like scripting language for register/memory
expressions. It is built on top of
[`jplcz_microfmt`](https://github.com/jplcz/microfmt)'s inspector primitives
(`address_space_ref`, `register_context_ref`) for fault-safe target access
and formatted output.

It was split out of `jplcz_microfmt` into its own repository so it can evolve
and version independently.

See [docs/micro-vm.md](docs/micro-vm.md) for the full guide.

## Getting the code

External users should clone over HTTPS:

```sh
git clone https://github.com/jplcz/microvm.git
```

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

By default the build fetches [`jplcz_microfmt`](https://github.com/jplcz/microfmt)
via CMake `FetchContent`, preferring a sibling `../microfmt` checkout if one
exists, otherwise cloning it from GitHub. `JPLCZ_MICROVM_BUILD_TESTS` and
`JPLCZ_MICROVM_BUILD_EXAMPLES` control the test suite and example programs
(both default on for top-level builds).

## Using it as a dependency

The CMake package name is `jplcz_microvm`, exposing a single target:

```cmake
target_link_libraries(your_target PRIVATE jplcz_microvm::microvm)
```
