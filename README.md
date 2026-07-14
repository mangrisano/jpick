# jpick

A tiny [`jq`](https://stedolan.github.io/jq/)-like JSON tool written in C++20.

`jpick` reads JSON from standard input or a file, optionally extracts a value
with a simple path expression, and prints the result back as valid JSON
(compact or pretty-printed).

It was built as a learning project: a hand-written lexer, a recursive-descent
parser, a `std::variant`-based data model, a path query engine, and a
serializer.

## Features

- Hand-written JSON **lexer** and **recursive-descent parser**
- Query values with a path expression: object keys and **array indices**
- **Compact** or **pretty-printed** output
- Read from **stdin** or a **file**
- Clear error messages with a non-zero exit code on failure

## Build

Requirements: a C++20 compiler and CMake ≥ 3.20.

```bash
cmake -S . -B build
cmake --build build
```

The executable is produced at `build/jpick`.

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
  -p,--pretty   Pretty-print the output
```

## Examples

### Format JSON (no path)

```bash
echo '{"a":1,"b":[1,2]}' | ./build/jpick
```

```json
{ "a": 1, "b": [1, 2] }
```

### Pretty-print

Pretty mode puts every array element and object member on its own line,
indented by two spaces per nesting level:

```bash
echo '{"a":1,"b":[1,2]}' | ./build/jpick --pretty
```

```json
{
  "a": 1,
  "b": [1, 2]
}
```

### Extract an object field

```bash
echo '{"user":{"name":"anna","age":30}}' | ./build/jpick '.user.name'
```

```json
"anna"
```

### Index into an array

```bash
echo '{"users":["anna","luca","sara"]}' | ./build/jpick '.users[1]'
```

```json
"luca"
```

Paths combine keys and indices, including nested indices:

```bash
echo '{"matrix":[[1,2],[3,4]]}' | ./build/jpick '.matrix[1][0]'
```

```json
3
```

### Combine a path with pretty-print

```bash
echo '{"tags":["cli","json"],"count":2}' | ./build/jpick '.tags' --pretty
```

```json
["cli", "json"]
```

### Read from a file

```bash
echo '{"tags":["cli","json"],"count":2}' > data.json
./build/jpick '.tags' data.json
```

```json
["cli", "json"]
```

### Scalar values

Strings, numbers, booleans and `null` are printed as valid JSON:

```bash
echo '{"ok":true,"ratio":6.022e23,"missing":null}' | ./build/jpick '.ratio'
```

```json
6.022e23
```

### Errors

Invalid input, a missing field, or an out-of-range index print a message to
`stderr` and exit with status `1`:

```bash
echo '{"a":1}' | ./build/jpick '.b'
# jpick: Field does not exist   (exit code 1)

echo '{"a":[1]}' | ./build/jpick '.a[5]'
# jpick: Index out of range     (exit code 1)
```

## Path syntax

- `.key` — descend into an object by key
- `[n]` — index into an array (0-based)
- Steps can be chained: `.a.b[0].c[1][2]`
- An empty path (or none) selects the whole document

## Project layout

```
src/
  json.hpp        # Value data model (std::variant)
  lexer.hpp       # tokenizer
  parser.hpp      # recursive-descent parser
  query.hpp       # path parsing and tree navigation
  serializer.hpp  # compact and pretty serialization
  main.cpp        # CLI entry point
tests/
  test_jpick.cpp
third_party/
  doctest.h       # test framework
  CLI11.hpp       # command-line parsing
```

## Notes and limitations

- Object output key order is not preserved (objects use `std::unordered_map`).
  This is valid JSON, but keys may appear in a different order than the input.
- Unicode `\uXXXX` escape sequences are not decoded.
