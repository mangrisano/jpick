# Contributing to jpick

Thanks for your interest in improving jpick! This is a small learning project,
but contributions are welcome.

## Getting started

```bash
git clone https://github.com/mangrisano/jpick.git
cd jpick
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Requirements: a C++20 compiler and CMake ≥ 3.20.

## Guidelines

- **Build warning-free.** The project compiles with `-Wall -Wextra -Wpedantic`.
- **Add tests.** New behaviour should come with a `TEST_CASE` in
  [`tests/test_jpick.cpp`](tests/test_jpick.cpp). Run the suite before opening a PR.
- **Keep it focused.** One logical change per pull request.
- **Commit messages** follow the [Conventional Commits](https://www.conventionalcommits.org/)
  style, e.g. `feat(query): ...`, `fix(lexer): ...`, `docs(readme): ...`.
- **Source layout:** public headers live in `include/jpick/`, the CLI entry
  point in `src/main.cpp`, and third-party single headers in `third_party/`.

## Reporting bugs

Open an issue using the bug report template and include the input JSON, the
command you ran, and the expected vs. actual output.
