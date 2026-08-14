cmake_minimum_required(VERSION 3.20)
project(jpick VERSION 2.9.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Esporta build/compile_commands.json cosi' IntelliSense (e altri tool)
# usano gli stessi flag/standard del build reale.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(jpick src/main.cpp)
# Public headers live in include/ (as jpick/*.hpp); CLI11.hpp in third_party/.
target_include_directories(jpick PRIVATE include third_party)
target_compile_options(jpick PRIVATE -Wall -Wextra -Wpedantic)
# Expose the project version to the CLI (used by --version).
target_compile_definitions(jpick PRIVATE JPICK_VERSION="${PROJECT_VERSION}")

# ---- Install ----
# `cmake --install build` copia jpick in una cartella del PATH.
# GNUInstallDirs definisce CMAKE_INSTALL_BINDIR (di solito "bin"), relativo
# a CMAKE_INSTALL_PREFIX (default /usr/local -> /usr/local/bin).
include(GNUInstallDirs)
install(TARGETS jpick RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---- Test (doctest) ----
enable_testing()

add_executable(jpick_tests tests/test_jpick.cpp)
# Logic headers in include/ (as jpick/*.hpp), doctest.h in third_party/.
target_include_directories(jpick_tests PRIVATE include third_party)
target_compile_options(jpick_tests PRIVATE -Wall -Wextra -Wpedantic)

# Registra l'eseguibile di test con CTest, cosi' basta lanciare `ctest`.
add_test(NAME jpick_tests COMMAND jpick_tests)

# ---- Fuzzing (libFuzzer + ASan/UBSan) ----
# Off by default: fuzzing needs a Clang that ships libFuzzer. Apple clang does
# NOT — use the Homebrew LLVM toolchain. Configure a dedicated build dir with:
#
#   cmake -S . -B build-fuzz -DJPICK_FUZZ=ON \
#     -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
#   cmake --build build-fuzz
#   ./build-fuzz/jpick_fuzz_parse        # optionally pass a scratch corpus dir
#
option(JPICK_FUZZ "Build libFuzzer fuzzing harnesses" OFF)

if(JPICK_FUZZ)
    # -O1 keeps ASan builds reasonably fast; frame pointers give clean stacks.
    set(JPICK_FUZZ_FLAGS -g -O1 -fno-omit-frame-pointer
        -fsanitize=fuzzer,address,undefined)

    foreach(name IN ITEMS parse query roundtrip)
        add_executable(jpick_fuzz_${name} fuzz/fuzz_${name}.cpp)
        target_include_directories(jpick_fuzz_${name} PRIVATE include third_party)
        target_compile_options(jpick_fuzz_${name} PRIVATE ${JPICK_FUZZ_FLAGS})
        target_link_options(jpick_fuzz_${name} PRIVATE ${JPICK_FUZZ_FLAGS})
    endforeach()
endif()
