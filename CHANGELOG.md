# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-07-14

### Added

- Hand-written JSON **lexer** and recursive-descent **parser**.
- `std::variant`-based JSON data model (`Value`, `Array`, `Object`).
- Path queries with object keys and **array indices**, e.g. `.a.b[0]`.
- **Compact** and **pretty-printed** serialization (`-p`/`--pretty`).
- Read JSON from **stdin** or a **file**.
- `-v`/`--version` flag.
- CLI built with [CLI11](https://github.com/CLIUtils/CLI11); test suite with
  [doctest](https://github.com/doctest/doctest).
- `cmake --install` target to place the binary on the `PATH`.

[1.0.0]: https://github.com/mangrisano/jpick/releases/tag/v1.0.0
