# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Streaming input: when the input holds several JSON values (NDJSON or
  whitespace-separated), each is processed in turn — one output per input
  value, like `jq`.
- `-s`/`--slurp` flag: read all input values into a single array, so they can
  be aggregated (e.g. `jpick -s 'add'`), like `jq -s`.

### Changed

- Multiple top-level JSON values are now processed as a stream instead of
  raising an "Unexpected trailing content" error at the CLI.

## [2.1.0] - 2026-07-31

### Added

- Filter builtin **`select(expr)`**: keeps the current value when `expr` is
  truthy (anything other than `null` or `false`) and drops it otherwise, e.g.
  `.users[] | select(.active)` (like `jq`). Comparisons such as `.age > 18` are
  intentionally out of scope.
- Alternative operator **`//`**: yields its left-hand side unless that is
  `null`, `false`, or missing (or raises an error), otherwise falls back to the
  right-hand side. Alternatives can be chained, e.g. `.a // .b // 0` (like `jq`).
- Scalar literals `true`, `false`, `null`, and numbers can be used as an
  expression, e.g. as the fallback in `.x // 0` (like `jq`).
- Aggregate builtins **`add`** (sum numbers, concatenate strings/arrays, merge
  objects), **`min`**, **`max`**, **`first`**, **`last`** (like `jq`).
- Array builtins **`sort`**, **`unique`** (sort + dedupe) and **`reverse`**
  (also reverses strings), using jq's total value ordering (like `jq`).
- String/array builtins **`join("sep")`** and **`split("sep")`** (like `jq`).

### Changed

- Indexing a `null` value now returns `null` instead of erroring, so paths can
  descend past a missing branch (e.g. `.a.b` when `.a` is absent), matching `jq`.
- Refactored builtin dispatch to a lookup table, so adding a unary builtin is
  a one-line change (no behavior change).
- The pipe (`|`) and alternative (`//`) operators are now parenthesis-aware, so
  they are not split when they appear inside `select(...)`.

## [2.0.0] - 2026-07-31

### Added

- Builtin function **`length`**: returns the number of elements in an array or
  object, or the number of characters in a string. Other types return `null`.
  Usage: `jpick '.users | length'` (like `jq`).
- Builtin function **`keys`**: returns an array of an object's keys (sorted
  alphabetically) or an array's indices. Usage: `jpick 'keys'` (like `jq`).
- Builtin function **`type`**: returns the JSON type of a value as a string
  (`"null"`, `"boolean"`, `"number"`, `"string"`, `"array"`, or `"object"`).
  Usage: `jpick 'type'` (like `jq`).
- Builtin function **`has`**: tests whether an object has a given key, e.g.
  `jpick 'has("age")'`. Non-objects return `false` (like `jq`).
- Builtin function **`not`**: negates a boolean, e.g. `jpick 'has("age") | not'`
  (like `jq`).
- Builtin function **`empty`**: produces no output, removing the value from the
  stream (like `jq`).
- **Array slicing** `[start:end]`: extracts a sub-array, e.g. `jpick '.[1:3]'`.
  Both bounds are optional (`[:2]`, `[2:]`) and negative indices count from the
  end (`[-2:]`); out-of-range bounds are clamped (like `jq`).

### Changed

- **BREAKING**: Accessing a field that does not exist (e.g. `.missing`) now
  returns `null` instead of throwing an error, matching `jq` behavior. This
  makes it safe to query optional fields in real-world data where not all
  objects have the same shape.
- **BREAKING**: Accessing an array index that is out of range (e.g. `.[999]`)
  now returns `null` instead of throwing an error, matching `jq` behavior.

## [1.5.0] - 2026-07-20

### Added

- Output format filters `@base64d` (base64 decode), `@uri` (percent-encoding),
  and `@sh` (POSIX shell quoting), usable as a pipe stage, like `jq`.

## [1.4.0] - 2026-07-20

### Added

- `--indent N` and `--tab` options to control pretty-print indentation
  (N spaces or tabs per level); both imply `--pretty`, like `jq`.
- `-S`/`--sort-keys` option to emit object keys in ascending order
  (recursively), like `jq -S`.
- Output format filters `@text`, `@json`, `@base64`, `@csv`, and `@tsv`,
  usable as a pipe stage (e.g. `.rows[] | @csv`), like `jq`.

### Changed

- Objects now preserve the insertion order of their keys, so the serialized
  output mirrors the source document (key order was previously
  non-deterministic).

## [1.3.0] - 2026-07-20

### Added

- String interpolation: a `"..."` segment builds a string, replacing every
  `\(...)` with the value the inner path produces (e.g.
  `.items[] | "\(.n)=\(.c)"`), like `jq`. The inner expression must produce
  exactly one value.
- `-r`/`--raw-output` flag: print top-level strings without quotes or
  escaping, like `jq -r`.

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

[Unreleased]: https://github.com/mangrisano/jpick/compare/v2.1.0...HEAD
[2.1.0]: https://github.com/mangrisano/jpick/releases/tag/v2.1.0
[2.0.0]: https://github.com/mangrisano/jpick/releases/tag/v2.0.0
[1.5.0]: https://github.com/mangrisano/jpick/releases/tag/v1.5.0
[1.4.0]: https://github.com/mangrisano/jpick/releases/tag/v1.4.0
[1.3.0]: https://github.com/mangrisano/jpick/releases/tag/v1.3.0
[1.2.0]: https://github.com/mangrisano/jpick/releases/tag/v1.2.0
[1.1.0]: https://github.com/mangrisano/jpick/releases/tag/v1.1.0
[1.0.0]: https://github.com/mangrisano/jpick/releases/tag/v1.0.0
