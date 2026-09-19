# SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
#
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.20)
project(jplcz_microvm VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(JPLCZ_MICROVM_IS_TOP_LEVEL ON)
else()
    set(JPLCZ_MICROVM_IS_TOP_LEVEL OFF)
endif()

option(JPLCZ_MICROVM_BUILD_TESTS "Build test suite" ${JPLCZ_MICROVM_IS_TOP_LEVEL})
option(JPLCZ_MICROVM_BUILD_EXAMPLES "Build example apps" ${JPLCZ_MICROVM_IS_TOP_LEVEL})
option(JPLCZ_MICROVM_INSTALL
    "Install jplcz_microvm headers and CMake package metadata (requires jplcz_microfmt \
provided via find_package, e.g. Conan/vcpkg/CPM, not the local FetchContent fallback)"
    OFF)

include(FetchContent)

# jplcz_microvm is a small register/memory scripting VM layered on top of the
# jplcz_microfmt inspector headers (address_space, register_context, ...).
# When developed side-by-side with microfmt, prefer the local checkout;
# otherwise fetch it from GitHub.
if(NOT TARGET jplcz_microfmt::microfmt)
    set(JPLCZ_MICROVM_MICROFMT_LOCAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../microfmt"
        CACHE PATH "Path to a local jplcz_microfmt checkout")

    if(EXISTS "${JPLCZ_MICROVM_MICROFMT_LOCAL_PATH}/CMakeLists.txt")
        set(JPLCZ_MICROFMT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(JPLCZ_MICROFMT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(JPLCZ_MICROFMT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        set(JPLCZ_MICROFMT_BUILD_HEADER_CHECKS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(jplcz_microfmt SOURCE_DIR "${JPLCZ_MICROVM_MICROFMT_LOCAL_PATH}")
    else()
        FetchContent_Declare(jplcz_microfmt
            GIT_REPOSITORY git@github.com:jplcz/microfmt.git
            GIT_TAG master)
    endif()

    FetchContent_MakeAvailable(jplcz_microfmt)
endif()

add_library(jplcz_microvm INTERFACE)
add_library(jplcz_microvm::microvm ALIAS jplcz_microvm)
set_target_properties(jplcz_microvm PROPERTIES EXPORT_NAME microvm)

target_include_directories(jplcz_microvm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(jplcz_microvm INTERFACE jplcz_microfmt::microfmt)
target_compile_features(jplcz_microvm INTERFACE cxx_std_20)

if(JPLCZ_MICROVM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(JPLCZ_MICROVM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(JPLCZ_MICROVM_INSTALL)
    include(GNUInstallDirs)
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS jplcz_microvm EXPORT jplcz_microvmTargets)
    install(EXPORT jplcz_microvmTargets
        FILE jplcz_microvmTargets.cmake
        NAMESPACE jplcz_microvm::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jplcz_microvm)

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/jplcz_microvm-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/jplcz_microvm-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jplcz_microvm)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/jplcz_microvm-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/jplcz_microvm-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/jplcz_microvm-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jplcz_microvm)
endif()
