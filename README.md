<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/mangrisano/jpick/main/docs/logo-dark.svg">
  <img alt="jpick" src="https://raw.githubusercontent.com/mangrisano/jpick/main/docs/logo.svg" width="240">
</picture>

[![CI](https://github.com/mangrisano/jpick/actions/workflows/ci.yml/badge.svg)](https://github.com/mangrisano/jpick/actions/workflows/ci.yml)
[![Performance](https://github.com/mangrisano/jpick/actions/workflows/performance.yml/badge.svg)](https://github.com/mangrisano/jpick/actions/workflows/performance.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

</div>

A tiny [`jq`](https://stedolan.github.io/jq/)-like JSON tool written in C++20.

`jpick` reads JSON from standard input or a file, optionally extracts a value
with a simple path expression, and prints the result back as valid JSON
(compact or pretty-printed).

It is a small, dependency-free binary built from scratch: a hand-written lexer,
a recursive-descent parser, a `std::variant`-based data model, a path query
engine, and a serializer — the querying essentials of `jq`, without the runtime.

<img src="https://raw.githubusercontent.com/mangrisano/jpick/main/docs/demo.gif" alt="jpick demo" width="720">

## Contents

- [Features](#features)
- [Differences from `jq`](#differences-from-jq)
- [Install](#install)
- [Build](#build)
- [Usage](#usage)
- [Examples](#examples)
- [Path syntax](#path-syntax)
- [Project layout](#project-layout)
- [Notes and limitations](#notes-and-limitations)
- [License](#license)

## Features

- Hand-written JSON **lexer** and **recursive-descent parser**
- **jq-compatible** behavior: missing fields and out-of-range indices return `null` instead of an error
- Query values with a path expression: object keys, **array indices**, **slices** (`[1:3]`), and **iteration** (`[]`)
- Compose queries with the **pipe** operator (`|`)
- Provide defaults with the **alternative** operator (`//`): `.price // 0`
- **Filter** a stream with `select(...)`: `.users[] | select(.active)`
- **Transform** each element with `map(...)`: `map(.price) | add`
- **Construct arrays** with `[ ... ]`: collect a whole stream, e.g. `[.users[].name]`
- **Compare** values with `==`, `!=`, `<`, `<=`, `>`, `>=`: `.users[] | select(.age >= 18)`
- Decode embedded JSON with **`fromjson`** (and encode with `@json`)
- **Builtin functions**: `length`, `keys`, `to_entries`, `from_entries`, `type`, `has`, `contains`, `not`, `empty`, `add`, `sort`, `unique`, `reverse`, `min`, `max`, `first`, `last`, `join`, `split`, `tonumber`, `tostring`, `fromjson`, `ascii_downcase`, `ascii_upcase`, `ltrimstr`, `rtrimstr`, `startswith`, `endswith` (jq-compatible)
- Build strings with **interpolation**: `"\(.name): \(.count)"`
- Format output with `@text`, `@json`, `@base64`, `@base64d`, `@base32`, `@base32d`, `@uri`, `@html`, `@sh`, `@csv`, `@tsv`, like `jq`
- **Compact** or **pretty-printed** output, with configurable indentation (`--indent`, `--tab`)
- **Sort object keys** with `-S`/`--sort-keys`
- **Raw** string output (`-r`/`--raw-output`), like `jq -r`
- Process a **stream of JSON values** (NDJSON), or combine them with `-s`/`--slurp`
- Read **plain text** with `-R`/`--raw-input`: each line becomes a string, like `jq -R`
- Read from **stdin** or a **file**
- Clear error messages with a non-zero exit code on failure

## Differences from `jq`

`jpick` covers the querying and extraction essentials of `jq`, but it is a
focused extractor and light transformer, **not** a full programming language.
The table below summarizes what it has and what it leaves to `jq`.

| Area                  | In `jpick`                                                                                                                                                                                                                                                                                  | Not in `jpick` (use `jq`)                                                             |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| Navigation            | `.key`, `[n]`, `[a:b]`, `[]`                                                                                                                                                                                                                                                                | recursive descent `..`, optional `.a?`                                                |
| Composition & flow    | pipe `\|`, alternative `//`, `select(...)`, `map(...)`                                                                                                                                                                                                                                      | `if/then/else`, `try/catch`, `reduce`, `foreach`                                      |
| Arithmetic & logic    | comparisons `== != < <= > >=` (e.g. `select(.age > 18)`)                                                                                                                                                                                                                                    | `+ - * / %`, `and`/`or`                                                               |
| Construction          | array `[ ... ]`, comma `.a, .b` (e.g. `[.users[].name]`)                                                                                                                                                                                                                                    | object `{a: .x}`                                                                      |
| Variables & functions | —                                                                                                                                                                                                                                                                                           | `... as $x`, `def`                                                                    |
| Update / assignment   | —                                                                                                                                                                                                                                                                                           | `=`, `\|=`, `+=`, `del(...)`, `getpath`/`setpath`, `paths`                            |
| Regular expressions   | —                                                                                                                                                                                                                                                                                           | `test`, `match`, `capture`, `scan`, `sub`, `gsub`                                     |
| Builtins              | `length`, `keys`, `type`, `has`, `not`, `empty`, `add`, `sort`, `unique`, `reverse`, `min`, `max`, `first`, `last`, `join`, `split`, `tonumber`, `tostring`, `fromjson`, `to_entries`/`from_entries`, `contains`, `ascii_downcase`/`upcase`, `ltrimstr`/`rtrimstr`, `startswith`/`endswith` | `sort_by`, `group_by`, `unique_by`, `flatten`, `range`, `walk`, math & date functions |
| Strings               | interpolation `"\(...)"` (exactly one value)                                                                                                                                                                                                                                                | regex-based string ops (see above)                                                    |
| Formats               | `@text`, `@json`, `@base64`/`@base64d`, `@base32`/`@base32d`, `@uri`, `@html`, `@sh`, `@csv`, `@tsv`                                                                                                                                                                                        | —                                                                                     |
| Output                | **compact by default**, `--pretty`, `--indent`/`--tab`, `-S`/`--sort-keys`, `-r`/`--raw-output`                                                                                                                                                                                             | pretty-printed by default                                                             |
| Input                 | stdin or file, NDJSON stream, `-s`/`--slurp`, `-R`/`--raw-input`                                                                                                                                                                                                                            | `input`/`inputs`, `env`/`$ENV`, `--arg`/`--argjson`/`--stream`                        |

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
  -s,--slurp       Read all inputs into a single array
  -R,--raw-input   Read each input line as a string instead of parsing JSON
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
lack the field instead of erroring. The inner expression can also use the
comparison operators `==`, `!=`, `<`, `<=`, `>` and `>=`:

```bash
echo '[{"name":"anna","age":30},{"name":"luca","age":16}]' \
  | jpick -r '.[] | select(.age >= 18) | .name'
```

```text
anna
```

### Transform with `map`

`map(expr)` applies `expr` to every element of an array and collects the
results into a new array — the idiomatic way to reshape or aggregate (like
`jq`). It composes with paths, `select`, pipes and builtins:

```bash
echo '[{"price":9},{"price":3},{"price":7}]' | jpick 'map(.price) | add'
```

```text
19
```

```bash
echo '[{"a":1,"ok":true},{"a":2,"ok":false},{"a":3,"ok":true}]' \
  | jpick 'map(select(.ok) | .a)'
```

```text
[1, 3]
```

### Construct an array

Wrap an expression in `[ ... ]` to gather its entire output stream into a
single array — the way to turn many results back into one value, or to feed a
whole stream to a builtin like `add` (like `jq`):

```bash
echo '{"users":[{"name":"anna"},{"name":"luca"}]}' | jpick '[.users[].name]'
```

```text
["anna", "luca"]
```

Top-level commas build a fixed-shape array, and the result can be piped onward:

```bash
echo '{"assets":[{"n":1},{"n":2},{"n":3}]}' | jpick '[.assets[].n] | add'
```

```text
6
```

### Decode embedded JSON with `fromjson`

`fromjson` parses a string that contains JSON into a real value — the inverse
of `@json`. This is handy for logs where a field is a JSON-encoded string:

```bash
echo '{"payload":"{\"id\":42}"}' | jpick '.payload | fromjson | .id'
```

```text
42
```

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

### Process multiple JSON values (NDJSON)

When the input holds several JSON values (newline-delimited or just
whitespace-separated), `jpick` processes each of them in turn — one output per
input value, like `jq`:

```bash
printf '{"a":1}\n{"a":2}\n{"a":3}\n' | jpick '.a'
```

```text
1
2
3
```

`-s`/`--slurp` instead reads **all** input values into a single array, so you
can aggregate across them:

```bash
printf '1\n2\n3\n4\n' | jpick -s 'add'
```

```text
10
```

More `--slurp` patterns — count the values, take the extremes, or sort them:

```bash
# how many log lines?
printf '{"a":1}\n{"a":2}\n{"a":3}\n' | jpick -s 'length'
```

```text
3
```

```bash
# highest value in a numeric stream
printf '3\n1\n4\n1\n5\n' | jpick -s 'max'
```

```text
5
```

```bash
# sort and de-duplicate a stream
printf '3\n1\n2\n1\n3\n' | jpick -s 'unique'
```

```text
[1, 2, 3]
```

```bash
# first / last value of the stream
printf '10\n20\n30\n' | jpick -s 'first'
```

```text
10
```

Because each NDJSON value is processed on its own, aggregating over a _field_
across the whole stream is a two-stage pipe: extract the field from each value,
then slurp the results. For example, to sort every user's name across
newline-delimited objects:

```bash
printf '{"n":"c"}\n{"n":"a"}\n{"n":"b"}\n' | jpick -s '.[].n' | jpick -s 'sort'
```

```text
["a", "b", "c"]
```

### Read plain text with `-R`

By default `jpick` parses its input as JSON. `-R`/`--raw-input` instead treats
each input line as a string (like `jq -R`), so the string builtins can process
logs and other non-JSON text:

```bash
printf 'v1.2.0\nv1.3.0\n' | jpick -R -r 'ltrimstr("v")'
```

```text
1.2.0
1.3.0
```

Because each line is now a string, it composes with `select`, `split` and the
`@` formats:

```bash
printf 'ERROR: boom\nINFO: ok\nERROR: bad\n' | jpick -R -r 'select(startswith("ERROR"))'
```

```text
ERROR: boom
ERROR: bad
```

Combined with `-s`/`--slurp`, the **whole** input becomes a single string
(like `jq -Rs`):

```bash
echo 'hello' | jpick -R -s -r '@base64'
```

```text
aGVsbG8K
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

### Builtins: `tonumber`, `tostring`

Convert between numbers and strings. `tonumber` parses a string (or passes a
number through); `tostring` renders any value as a string:

```bash
echo '"42"' | jpick 'tonumber'
```

```text
42
```

This is handy when numbers arrive as strings — parse them, then aggregate:

```bash
printf '"10"\n"20"\n"12"\n' | jpick 'tonumber' | jpick -s 'add'
```

```text
42
```

### Builtins: `ascii_downcase`, `ascii_upcase`

Change the case of the ASCII letters in a string:

```bash
echo '"Hello World"' | jpick -r 'ascii_downcase'
```

```text
hello world
```

### Builtins: `ltrimstr`, `rtrimstr`

Strip a prefix or suffix if present (the value is left unchanged otherwise) —
useful for tags, versions and file names:

```bash
echo '"v2.1.0"' | jpick -r 'ltrimstr("v")'
```

```text
2.1.0
```

```bash
echo '"report.csv"' | jpick -r 'rtrimstr(".csv")'
```

```text
report
```

### Builtins: `startswith`, `endswith`

Test whether a string begins or ends with a substring. They pair naturally with
`select` and `map` to filter by name:

```bash
echo '["a.txt","b.csv","c.txt"]' | jpick 'map(select(endswith(".txt")))'
```

```text
["a.txt", "c.txt"]
```

### Builtins: `to_entries`, `from_entries`

`to_entries` turns an object into an array of `{"key":…, "value":…}` records
and `from_entries` turns it back. Together with `map` and `select` they let you
filter or reshape an object by its keys or values:

```bash
echo '{"a":1,"b":2,"c":3}' | jpick 'to_entries | map(select(.value > 1)) | from_entries'
```

```text
{"b": 2, "c": 3}
```

### Builtins: `contains`

`contains(x)` tests deep containment, like `jq`: a string contains a substring,
an array contains another when each of its elements is contained somewhere, and
an object contains another when every key matches recursively. The argument `x`
is a JSON literal:

```bash
echo '["apple","banana","grape"]' | jpick 'map(select(contains("ap")))'
```

```text
["apple", "grape"]
```

### Format with `@`

A pipe stage starting with `@` formats each value. `@csv`/`@tsv` take an array
of scalars; `@base64`, `@base64d`, `@base32`, `@base32d`, `@uri`, `@html`,
`@sh`, `@json` and `@text` take any value (`@sh` also accepts an array).
Combine with `-r` for clean output.

One example per format (each run as `echo '<input>' | jpick -r '. | @<fmt>'`):

| Format     | Input                   | Output                       |
| ---------- | ----------------------- | ---------------------------- |
| `@text`    | `42`                    | `42`                         |
| `@json`    | `{"a":1,"b":[2,3]}`     | `{"a": 1, "b": [2, 3]}`      |
| `@base64`  | `"hello"`               | `aGVsbG8=`                   |
| `@base64d` | `"aGVsbG8="`            | `hello`                      |
| `@base32`  | `"hello"`               | `NBSWY3DP`                   |
| `@base32d` | `"NBSWY3DP"`            | `hello`                      |
| `@uri`     | `"a b/c?x=1"`           | `a%20b%2Fc%3Fx%3D1`          |
| `@html`    | `"<b>R&D</b>"`          | `&lt;b&gt;R&amp;D&lt;/b&gt;` |
| `@sh`      | `"it's ok"`             | `'it'\''s ok'`               |
| `@csv`     | `["anna",30,true,null]` | `"anna",30,true,`            |
| `@tsv`     | `["anna",30,true,null]` | `anna<tab>30<tab>true<tab>`  |

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
- `[ ... ]` — construct an array by collecting the inner stream (e.g. `[.users[].name]`)
- `|` — pipe: feed every result of one stage into the next
- `//` — alternative: fall back when the left side is `null`, `false`, or missing
- `length`, `keys`, `type`, `has("key")`, `not`, `empty` — builtin functions
- `add`, `sort`, `unique`, `reverse`, `min`, `max`, `first`, `last` — aggregate builtins
- `join("sep")`, `split("sep")`, `ltrimstr("s")`, `rtrimstr("s")` — string builtins
- `startswith("s")`, `endswith("s")` — string predicates (pair with `select`)
- `tonumber`, `tostring`, `ascii_downcase`, `ascii_upcase` — conversion/case builtins
- `fromjson` — parse a JSON string into a value (inverse of `@json`)
- `to_entries`, `from_entries` — convert between an object and an array of `{key, value}` entries
- `contains(x)` — deep containment: substring, subarray, or object subset (`x` is a JSON literal)
- `select(expr)` — keep the value when `expr` is truthy, drop it otherwise
- `map(expr)` — apply `expr` to every array element, collecting the results
- `"..."` — a string literal; `\(...)` interpolates the value of an inner path
- `@fmt` — format a value: `@text`, `@json`, `@base64`, `@base64d`, `@base32`, `@base32d`, `@uri`, `@html`, `@sh`, `@csv`, `@tsv`
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
