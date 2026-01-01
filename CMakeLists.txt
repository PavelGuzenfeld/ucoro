cmake_minimum_required(VERSION 3.22)
project(ucoro
    VERSION 0.0.1
    DESCRIPTION "ucoro coroutine library"
    LANGUAGES CXX
)

# --- Fix for CMake 3.30+ ---
# Dependencies like doctest use older cmake_minimum_required (e.g. 3.0).
# CMake 3.30+ errors on versions < 3.5. This variable forces the minimum to 3.5.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

option(UCORO_BUILD_TESTS "Build tests" ON)
option(UCORO_BUILD_BENCHMARKS "Build benchmarks" ON)
option(UCORO_BUILD_EXAMPLES "Build examples" ON)
option(UCORO_ENABLE_SANITIZERS "Enable ASan/UBSan in debug builds" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Link Time Optimization (LTO)
include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)
if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    message(STATUS "IPO/LTO enabled")
else()
    message(WARNING "IPO/LTO not supported: ${output}")
endif()

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

# --- Dependencies ---
include(FetchContent)

# 1. fmt (Replacement for <print> and <format>)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        10.2.1
)
FetchContent_MakeAvailable(fmt)

# 2. doctest (For testing)
if(UCORO_BUILD_TESTS)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)
endif()

# 3. Boost (Optional, for benchmarks)
find_package(Boost COMPONENTS context QUIET)

# --- Library Target ---
add_library(ucoro INTERFACE)
add_library(ucoro::ucoro ALIAS ucoro)

target_include_directories(ucoro INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(ucoro INTERFACE cxx_std_23)

# Link fmt to the library interface so users get it automatically
target_link_libraries(ucoro INTERFACE fmt::fmt)

# --- Tests ---
if(UCORO_BUILD_TESTS)
    enable_testing()
    add_executable(test_ucoro tests/test_ucoro.cpp)
    target_link_libraries(test_ucoro PRIVATE ucoro doctest::doctest)
    target_include_directories(test_ucoro PRIVATE tests)
    add_test(NAME ucoro_tests COMMAND test_ucoro)
endif()

# --- Benchmarks ---
if(UCORO_BUILD_BENCHMARKS)
    add_executable(benchmark_ucoro benchmarks/benchmark_ucoro.cpp)
    target_link_libraries(benchmark_ucoro PRIVATE ucoro)
    
    # Fix for MSVC optimization flags
    if(MSVC)
        target_compile_options(benchmark_ucoro PRIVATE $<$<CONFIG:Release>:/O2> $<$<CONFIG:Debug>:/Od>)
    else()
        target_compile_options(benchmark_ucoro PRIVATE $<$<CONFIG:Release>:-O3> $<$<CONFIG:Debug>:-O2>)
    endif()
    
    if(UNIX)
        target_compile_definitions(benchmark_ucoro PRIVATE HAVE_UCONTEXT)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(benchmark_ucoro PRIVATE -Wno-deprecated-declarations)
        endif()
    endif()

    if(Boost_FOUND)
        target_link_libraries(benchmark_ucoro PRIVATE Boost::context)
        target_compile_definitions(benchmark_ucoro PRIVATE HAVE_BOOST_CONTEXT)
    endif()
endif()

# --- Examples ---
if(UCORO_BUILD_EXAMPLES)
    add_executable(example_basic examples/basic.cpp)
    target_link_libraries(example_basic PRIVATE ucoro)
    
    add_executable(example_generator examples/generator.cpp)
    target_link_libraries(example_generator PRIVATE ucoro)
endif()

# --- Install ---
include(GNUInstallDirs)

install(TARGETS ucoro
    EXPORT ucoro-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES 
    include/ucoro/ucoro.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ucoro
)

install(EXPORT ucoro-targets
    FILE ucoro-config.cmake
    NAMESPACE ucoro::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ucoro
)