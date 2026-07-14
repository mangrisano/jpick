# jpick

[![CI](https://github.com/mangrisano/jpick/actions/workflows/ci.yml/badge.svg)](https://github.com/mangrisano/jpick/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

A tiny [`jq`](https://stedolan.github.io/jq/)-like JSON tool written in C++20.

`jpick` reads JSON from standard input or a file, optionally extracts a value
with a simple path expression, and prints the result back as valid JSON
(compact or pretty-printed).

It was built as a learning project: a hand-written lexer, a recursive-descent
parser, a `std::variant`-based data model, a path query engine, and a
serializer.

## Features

- Hand-written JSON **lexer** and **recursive-descent parser**
- Query values with a path expression: object keys, **array indices**, and **iteration** (`[]`)
- Compose queries with the **pipe** operator (`|`)
- **Compact** or **pretty-printed** output
- Read from **stdin** or a **file**
- Clear error messages with a non-zero exit code on failure

## Install

### Homebrew (macOS / Linux)

```bash
brew install mangrisano/jpick/jpick
```

### From a release

Prebuilt binaries for Linux and macOS are attached to each
[release](https://github.com/mangrisano/jpick/releases). Download the one for
your platform, verify its checksum, make it executable, and move it onto your
`PATH`:

```bash
chmod +x jpick-macos-arm64
sudo mv jpick-macos-arm64 /usr/local/bin/jpick
```

### From source

See [Build](#build) below.

## Build

Requirements: a C++20 compiler and CMake ≥ 3.20.

```bash
cmake -S . -B build
cmake --build build
```

The executable is produced at `build/jpick`.

The examples below call it simply as `jpick`. To use it that way, either run it
as `./build/jpick`, or install it onto your `PATH`:

```bash
cmake --install build                          # -> /usr/local/bin/jpick (may need sudo)
cmake --install build --prefix ~/.local        # -> ~/.local/bin/jpick (no sudo)
```

### Run the tests

```bash
ctest --test-dir build --output-on-failure
```

## Usage

```
jpick [OPTIONS] [path] [file]

Positionals:
  path TEXT     Query path, e.g. '.a.b[0]'   (default: whole document)
  file TEXT     JSON file to read            (default: stdin)

Options:
  -h,--help     Print help and exit
  -v,--version  Print version and exit
  -p,--pretty   Pretty-print the output
```

> Output blocks below are shown as literal terminal output.

## Examples

### Format JSON (no path)

```bash
echo '{"a":1,"b":[1,2]}' | jpick
```

```text
{"a": 1, "b": [1, 2]}
```

### Pretty-print

Pretty mode puts every array element and object member on its own line,
indented by two spaces per nesting level:

```bash
echo '{"a":1,"b":[1,2]}' | jpick --pretty
```

```text
{
  "a": 1,
  "b": [
    1,
    2
  ]
}
```

### Extract an object field

```bash
echo '{"user":{"name":"anna","age":30}}' | jpick '.user.name'
```

```text
"anna"
```

### Index into an array

```bash
echo '{"users":["anna","luca","sara"]}' | jpick '.users[1]'
```

```text
"luca"
```

Paths combine keys and indices, including nested indices:

```bash
echo '{"matrix":[[1,2],[3,4]]}' | jpick '.matrix[1][0]'
```

```text
3
```

### Iterate over an array

`[]` expands an array into one result per element (like `jq`). Following steps
are applied to each element:

```bash
echo '{"users":[{"name":"anna"},{"name":"luca"}]}' | jpick '.users[].name'
```

```text
"anna"
"luca"
```

### Pipe

The pipe operator `|` feeds every result of one stage into the next. `.a | .b`
is the same as `.a.b`, but pipes also let you compose stages freely:

```bash
echo '{"users":[{"name":"anna"},{"name":"luca"}]}' | jpick '.users[] | .name'
```

```text
"anna"
"luca"
```

### Combine a path with pretty-print

```bash
echo '{"tags":["cli","json"],"count":2}' | jpick '.tags' --pretty
```

```text
[
  "cli",
  "json"
]
```

### Read from a file

```bash
echo '{"tags":["cli","json"],"count":2}' > data.json
jpick '.tags' data.json
```

```text
["cli", "json"]
```

### Scalar values

Strings, numbers, booleans and `null` are printed as valid JSON:

```bash
echo '{"ok":true,"ratio":6.022e23,"missing":null}' | jpick '.ratio'
```

```text
6.022e+23
```

### Errors

Invalid input, a missing field, or an out-of-range index print a message to
`stderr` and exit with status `1`:

```bash
echo '{"a":1}' | jpick '.b'
# jpick: Field does not exist   (exit code 1)

echo '{"a":[1]}' | jpick '.a[5]'
# jpick: Index out of range     (exit code 1)
```

## Path syntax

- `.key` — descend into an object by key
- `[n]` — index into an array (0-based)
- `[]` — iterate over every element of an array (one result per element)
- `|` — pipe: feed every result of one stage into the next
- Steps can be chained: `.a.b[0].c[1][2]`, `.users[].name`, `.users[] | .name`
- An empty path (or none) selects the whole document

## Project layout

```
include/jpick/
  json.hpp        # Value data model (std::variant)
  lexer.hpp       # tokenizer
  parser.hpp      # recursive-descent parser
  query.hpp       # path parsing and tree navigation
  serializer.hpp  # compact and pretty serialization
src/
  main.cpp        # CLI entry point
tests/
  test_jpick.cpp
third_party/
  doctest.h       # test framework
  CLI11.hpp       # command-line parsing
```

All library code lives in the `jpick` namespace.

## Notes and limitations

- Object output key order is not preserved (objects use `std::unordered_map`).
  This is valid JSON, but keys may appear in a different order than the input.
- Unicode `\uXXXX` escape sequences are not decoded.

## License

Released under the [MIT License](LICENSE).
