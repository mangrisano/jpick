# Roadmap

`jpick` is a tiny, dependency-free `jq`-like JSON tool written in C++20. It aims
to cover the **querying essentials of `jq`** — navigation, iteration, pipes and
a focused set of builtins — in a single small binary, without the weight of a
full expression language.

This document tracks what has shipped and what is being considered next.

## Shipped

- JSON **lexer**, recursive-descent **parser** and `std::variant` data model.
- Path queries: object keys, array **indices**, **slices** (`[a:b]`) and
  **iteration** (`[]`).
- **Pipe** (`|`) and **alternative** (`//`) operators.
- **Filtering** with `select(...)` and **transformation** with `map(...)`.
- Builtins: `length`, `keys`, `type`, `has`, `not`, `empty`, `add`, `sort`,
  `unique`, `reverse`, `min`, `max`, `first`, `last`, `join`, `split`,
  `tonumber`, `tostring`, `fromjson`, `ascii_downcase`, `ascii_upcase`,
  `ltrimstr`, `rtrimstr`.
- String **interpolation** (`"\(.name)"`) and `@` formats (`@csv`, `@tsv`,
  `@json`, `@base64`, `@base64d`, `@base32`, `@base32d`, `@uri`, `@html`,
  `@sh`).
- Output control: compact / pretty / raw, custom indent, sorted keys.
- **Streaming input** (NDJSON) and `--slurp` to collect inputs into one array.
- Reads from **stdin** or a **file**; clear errors and non-zero exit codes.
- CI (Linux + macOS), test suite (doctest), Homebrew tap and release binaries.

## Considering next

- String predicates `startswith("s")` / `endswith("s")` (pair well with
  `select`).
- `flatten` to collapse nested arrays one or more levels deep.

## Non-goals

To stay a **lite** alternative rather than a second `jq`, the following are
intentionally out of scope:

- A full expression language: infix comparisons (`==`, `<`, `>`), boolean logic
  (`and`/`or`), arithmetic and conditionals (`if/then/else`).
- Assignment, variables, `reduce`, and function definitions.

For those, use [`jq`](https://stedolan.github.io/jq/).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Issues and pull requests are welcome.
