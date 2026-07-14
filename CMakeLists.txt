cmake_minimum_required(VERSION 3.20)
project(jpick LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Esporta build/compile_commands.json cosi' IntelliSense (e altri tool)
# usano gli stessi flag/standard del build reale.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(jpick src/main.cpp)
# CLI11.hpp sta in third_party/.
target_include_directories(jpick PRIVATE third_party)
target_compile_options(jpick PRIVATE -Wall -Wextra -Wpedantic)

# ---- Install ----
# `cmake --install build` copia jpick in una cartella del PATH.
# GNUInstallDirs definisce CMAKE_INSTALL_BINDIR (di solito "bin"), relativo
# a CMAKE_INSTALL_PREFIX (default /usr/local -> /usr/local/bin).
include(GNUInstallDirs)
install(TARGETS jpick RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---- Test (doctest) ----
enable_testing()

add_executable(jpick_tests tests/test_jpick.cpp)
# Gli header della logica stanno in src/, doctest.h in third_party/.
target_include_directories(jpick_tests PRIVATE src third_party)
target_compile_options(jpick_tests PRIVATE -Wall -Wextra -Wpedantic)

# Registra l'eseguibile di test con CTest, cosi' basta lanciare `ctest`.
add_test(NAME jpick_tests COMMAND jpick_tests)
