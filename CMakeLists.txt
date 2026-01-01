cmake_minimum_required(VERSION 3.22)
project(ucoro
    VERSION 0.0.1
    DESCRIPTION "ucoro coroutine library"
    LANGUAGES CXX
)

option(UCORO_BUILD_TESTS "Build tests" ON)
option(UCORO_BUILD_BENCHMARKS "Build benchmarks" ON)
option(UCORO_BUILD_EXAMPLES "Build examples" ON)
option(UCORO_ENABLE_SANITIZERS "Enable ASan/UBSan in debug builds" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wformat=2 -Wnull-dereference
        -Wdouble-promotion
    )
endif()

if(UCORO_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Dependencies
if(UCORO_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)
endif()

# Library Target
add_library(ucoro INTERFACE)
add_library(ucoro::ucoro ALIAS ucoro)

target_include_directories(ucoro INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(ucoro INTERFACE cxx_std_23)

# Tests
if(UCORO_BUILD_TESTS)
    enable_testing()
    add_executable(test_ucoro tests/test_ucoro.cpp)
    target_link_libraries(test_ucoro PRIVATE ucoro doctest::doctest)
    target_include_directories(test_ucoro PRIVATE tests)
    add_test(NAME ucoro_tests COMMAND test_ucoro)
endif()

# Benchmarks
if(UCORO_BUILD_BENCHMARKS)
    add_executable(benchmark_ucoro benchmarks/benchmark_ucoro.cpp)
    target_link_libraries(benchmark_ucoro PRIVATE ucoro)
    target_compile_options(benchmark_ucoro PRIVATE $<$<CONFIG:Release>:-O3> $<$<CONFIG:Debug>:-O2>)
endif()

# Examples
if(UCORO_BUILD_EXAMPLES)
    add_executable(example_basic examples/basic.cpp)
    target_link_libraries(example_basic PRIVATE ucoro)
    
    add_executable(example_generator examples/generator.cpp)
    target_link_libraries(example_generator PRIVATE ucoro)
endif()

# Install
include(GNUInstallDirs)

install(TARGETS ucoro
    EXPORT ucoro-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Only installing ucoro.hpp now
install(FILES 
    include/ucoro/ucoro.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ucoro
)

install(EXPORT ucoro-targets
    FILE ucoro-config.cmake
    NAMESPACE ucoro::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ucoro
)