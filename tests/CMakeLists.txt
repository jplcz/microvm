# SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
#
# SPDX-License-Identifier: BSD-2-Clause

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_jplcz_microvm_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE jplcz_microvm::microvm GTest::gtest_main)
    target_compile_features(${NAME} PRIVATE cxx_std_20)
    target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

add_jplcz_microvm_test(test_micro_vm test_micro_vm.cpp)
add_jplcz_microvm_test(test_variable_context test_variable_context.cpp)
add_jplcz_microvm_test(test_vm_label_allocator test_vm_label_allocator.cpp)
add_jplcz_microvm_test(test_vm_lexer test_vm_lexer.cpp)
