# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-07-14

### Added

- Pipe operator `|` in path expressions: each stage is applied to every value
  produced by the previous one (e.g. `.users[] | .name`), like `jq`.

## [1.1.0] - 2026-07-14

### Added

- Array iteration in path expressions: `[]` expands an array into one result
  per element (e.g. `.users[].name`), producing multiple outputs like `jq`.
- Richer `Value` API: convenience constructors, `is_*()`/`as_*()` accessors,
  `operator[]` navigation and `operator==`.

### Fixed

- The parser now rejects trailing content after a complete value (e.g.
  `[1][2]`) instead of silently ignoring it.

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

[1.2.0]: https://github.com/mangrisano/jpick/releases/tag/v1.2.0
[1.1.0]: https://github.com/mangrisano/jpick/releases/tag/v1.1.0
[1.0.0]: https://github.com/mangrisano/jpick/releases/tag/v1.0.0
