# SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
#
# SPDX-License-Identifier: BSD-2-Clause

function(add_microvm_example NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE jplcz_microvm::microvm)
    target_compile_features(${NAME} PRIVATE cxx_std_20)
endfunction()

add_microvm_example(vm_code_gen_demo vm_code_gen_demo.cpp)
add_microvm_example(vm_compiler_demo vm_compiler_demo.cpp)
