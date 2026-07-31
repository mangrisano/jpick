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
- **jq-compatible** behavior: missing fields and out-of-range indices return `null` instead of an error
- Query values with a path expression: object keys, **array indices**, **slices** (`[1:3]`), and **iteration** (`[]`)
- Compose queries with the **pipe** operator (`|`)
- Provide defaults with the **alternative** operator (`//`): `.price // 0`
- **Filter** a stream with `select(...)`: `.users[] | select(.active)`
- **Builtin functions**: `length`, `keys`, `type`, `has`, `not`, `empty`, `add`, `sort`, `unique`, `reverse`, `min`, `max`, `first`, `last`, `join`, `split` (jq-compatible)
- Build strings with **interpolation**: `"\(.name): \(.count)"`
- Format output with `@text`, `@json`, `@base64`, `@base64d`, `@uri`, `@sh`, `@csv`, `@tsv`, like `jq`
- **Compact** or **pretty-printed** output, with configurable indentation (`--indent`, `--tab`)
- **Sort object keys** with `-S`/`--sort-keys`
- **Raw** string output (`-r`/`--raw-output`), like `jq -r`
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
  -r,--raw-output  Output strings without quotes or escaping
  -S,--sort-keys   Sort object keys in the output
  --indent INT     Indent with N spaces (implies --pretty)
  --tab            Indent with tabs (implies --pretty)
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

### Slice an array

`[start:end]` extracts a sub-array (like `jq`). Both bounds are optional and
negative indices count from the end. Out-of-range bounds are clamped:

```bash
echo '[0,1,2,3,4]' | jpick '.[1:3]'
```

```text
[1, 2]
```

```bash
echo '[0,1,2,3,4]' | jpick '.[-2:]'
```

```text
[3, 4]
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

### Provide a default with `//`

The alternative operator `//` yields its left-hand side unless that is `null`,
`false`, or missing (or raises an error) — otherwise it falls back to the
right-hand side. It is the idiomatic way to supply defaults (like `jq`):

```bash
echo '{"name":"anna"}' | jpick '.age // 0'
```

```text
0
```

Alternatives can be chained; the first present value wins:

```bash
echo '{"nickname":"nino"}' | jpick -r '.name // .nickname // "anonymous"'
```

```text
nino
```

### Filter with `select`

`select(expr)` keeps the current value only when `expr` is truthy (anything
other than `null` or `false`), and drops it otherwise — the idiomatic way to
filter a stream (like `jq`). The inner expression can be any path, `has(...)`,
or a `//` fallback:

```bash
echo '{"users":[{"name":"anna","active":true},{"name":"luca","active":false}]}' \
  | jpick -r '.users[] | select(.active) | .name'
```

```text
anna
```

Because a missing field is `null` (falsy), `select` safely skips values that
lack the field instead of erroring. `jpick` keeps this deliberately small: for
comparisons like `select(.age > 18)`, reach for `jq`.

### Interpolate values into a string

A `"..."` segment builds a string, replacing every `\(...)` with the value the
inner path produces (like `jq`). The inner expression must yield exactly one
value:

```bash
echo '{"items":[{"n":"a","c":1},{"n":"b","c":2}]}' | jpick '.items[] | "\(.n)=\(.c)"'
```

```text
"a=1"
"b=2"
```

### Raw string output

By default strings are printed as valid JSON (quoted). `-r`/`--raw-output` prints
top-level strings without quotes or escaping; other values are unchanged:

```bash
echo '{"items":[{"n":"a","c":1},{"n":"b","c":2}]}' | jpick -r '.items[] | "\(.n)=\(.c)"'
```

```text
a=1
b=2
```

### Sort object keys

`-S`/`--sort-keys` emits object keys in ascending order (recursively). Without
it, keys keep the order of the source document:

```bash
echo '{"zebra":1,"apple":2,"mango":3}' | jpick -S
```

```text
{"apple": 2, "mango": 3, "zebra": 1}
```

### Choose the indentation

`--indent N` uses N spaces per level and `--tab` uses tabs; both imply
`--pretty`:

```bash
echo '{"a":[1,2]}' | jpick --indent 4
```

```text
{
    "a": [
        1,
        2
    ]
}
```

### Missing fields return `null`

Like `jq`, accessing a field that does not exist or an array index that is out
of range returns `null` instead of an error:

```bash
echo '{"a":1}' | jpick '.b'
```

```text
null
```

```bash
echo '[10,20]' | jpick '.[5]'
```

```text
null
```

This makes it safe to query optional fields in real-world data.

### Builtin: `length`

Returns the number of elements in an array or object, or the number of
characters in a string:

```bash
echo '{"users":[{"name":"anna"},{"name":"luca"},{"name":"sara"}]}' | jpick '.users | length'
```

```text
3
```

```bash
echo '"hello"' | jpick 'length'
```

```text
5
```

For other types, `length` returns `null`.

### Builtin: `keys`

Returns an array of an object's keys (sorted alphabetically) or an array's
indices:

```bash
echo '{"zebra":1,"apple":2}' | jpick 'keys'
```

```text
["apple", "zebra"]
```

```bash
echo '[10,20,30]' | jpick 'keys'
```

```text
[0, 1, 2]
```

### Builtin: `type`

Returns the JSON type of a value as a string:

```bash
echo '[1, "text", null, true, {}, []]' | jpick '.[] | type'
```

```text
"number"
"string"
"null"
"boolean"
"object"
"array"
```

### Builtin: `has`

Tests whether an object has a given key, returning `true` or `false`. Useful
for checking optional fields:

```bash
echo '{"name":"anna","age":30}' | jpick 'has("age")'
```

```text
true
```

### Builtin: `not`

Negates a boolean. Combine it with `has` to test for a missing key:

```bash
echo '{"name":"anna"}' | jpick 'has("age") | not'
```

```text
true
```

### Builtin: `empty`

Produces no output, removing the value from the stream. Useful to drop results:

```bash
echo '[1,2,3]' | jpick '.[] | empty'
```

```text

```

### Builtins: `add`, `min`, `max`

`add` sums an array of numbers, concatenates an array of strings or arrays, or
merges an array of objects. `min` and `max` return the smallest/largest element:

```bash
echo '[1,2,3,4]' | jpick 'add'
```

```text
10
```

```bash
echo '{"prices":[9,3,7]}' | jpick '.prices | max'
```

```text
9
```

### Builtins: `sort`, `unique`, `reverse`

`sort` orders an array, `unique` sorts and removes duplicates, `reverse`
reverses an array (or a string):

```bash
echo '[3,1,2,1]' | jpick 'unique'
```

```text
[1, 2, 3]
```

### Builtins: `first`, `last`

Return the first or last element of an array (`null` if empty):

```bash
echo '{"items":[10,20,30]}' | jpick '.items | first'
```

```text
10
```

### Builtins: `join`, `split`

`join(sep)` joins an array into a string; `split(sep)` splits a string into an
array. Pair `join` with `-r` for clean output:

```bash
echo '{"tags":["red","green","blue"]}' | jpick -r '.tags | join(", ")'
```

```text
red, green, blue
```

```bash
echo '"a,b,c"' | jpick 'split(",")'
```

```text
["a", "b", "c"]
```

### Format with `@`

A pipe stage starting with `@` formats each value. `@csv`/`@tsv` take an array
of scalars; `@base64`, `@base64d`, `@uri`, `@sh`, `@json` and `@text` take any
value (`@sh` also accepts an array). Combine with `-r` for clean output.

One example per format (each run as `echo '<input>' | jpick -r '. | @<fmt>'`):

| Format     | Input                   | Output                      |
| ---------- | ----------------------- | --------------------------- |
| `@text`    | `42`                    | `42`                        |
| `@json`    | `{"a":1,"b":[2,3]}`     | `{"a": 1, "b": [2, 3]}`     |
| `@base64`  | `"hello"`               | `aGVsbG8=`                  |
| `@base64d` | `"aGVsbG8="`            | `hello`                     |
| `@uri`     | `"a b/c?x=1"`           | `a%20b%2Fc%3Fx%3D1`         |
| `@sh`      | `"it's ok"`             | `'it'\''s ok'`              |
| `@csv`     | `["anna",30,true,null]` | `"anna",30,true,`           |
| `@tsv`     | `["anna",30,true,null]` | `anna<tab>30<tab>true<tab>` |

Turning an array of rows into CSV:

```bash
echo '[["anna",30,true],["luca",25,false]]' | jpick -r '.[] | @csv'
```

```text
"anna",30,true
"luca",25,false
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

Invalid JSON input, or applying a step to the wrong type (e.g. indexing a
number), prints a message to `stderr` and exits with status `1`:

```bash
echo 'not json' | jpick
# jpick: ...   (exit code 1)

echo '42' | jpick '.a'
# jpick: Value is not an Object   (exit code 1)
```

Accessing a missing field or an out-of-range index is **not** an error: it
returns `null`, like `jq` (see [Missing fields](#missing-fields-return-null)).

## Path syntax

- `.key` — descend into an object by key (missing keys return `null`)
- `[n]` — index into an array (0-based; out-of-range returns `null`)
- `[start:end]` — slice an array; bounds optional, negative indices allowed
- `[]` — iterate over every element of an array (one result per element)
- `|` — pipe: feed every result of one stage into the next
- `//` — alternative: fall back when the left side is `null`, `false`, or missing
- `length`, `keys`, `type`, `has("key")`, `not`, `empty` — builtin functions
- `add`, `sort`, `unique`, `reverse`, `min`, `max`, `first`, `last` — aggregate builtins
- `join("sep")`, `split("sep")` — string/array builtins
- `select(expr)` — keep the value when `expr` is truthy, drop it otherwise
- `"..."` — a string literal; `\(...)` interpolates the value of an inner path
- `@fmt` — format a value: `@text`, `@json`, `@base64`, `@base64d`, `@uri`, `@sh`, `@csv`, `@tsv`
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

- Object key order follows the source document; duplicate keys keep the last
  value. Use `-S`/`--sort-keys` to emit keys in ascending order instead.
- Unicode `\uXXXX` escape sequences are not decoded.

## License

Released under the [MIT License](LICENSE).
