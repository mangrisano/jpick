// Test suite for jpick.
//
// Uses doctest (third_party/doctest.h). The macro below tells doctest to
// generate the test program's main() for us: we only write the TEST_CASEs.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "jpick/json.hpp"
#include "jpick/lexer.hpp"
#include "jpick/parser.hpp"
#include "jpick/query.hpp"
#include "jpick/serializer.hpp"

using namespace jpick;

// Helper: from JSON text to the Value tree in one shot.
// Mirrors what main.cpp does: tokenize -> Parser -> parse_value.
static Value parse_json(const std::string &text)
{
    std::vector<Token> tokens = tokenize(text);
    Parser parser(tokens);
    return parser.parse();
}

// -----------------------------------------------------------------------------
// escape_string: the inverse of the lexer. Special chars -> escape sequences.
// -----------------------------------------------------------------------------
TEST_CASE("escape_string translates special characters")
{
    CHECK(escape_string("hello") == "hello");                // nothing to do
    CHECK(escape_string("a\"b") == "a\\\"b");                // quotes
    CHECK(escape_string("a\\b") == "a\\\\b");                // backslash
    CHECK(escape_string("line1\nline2") == "line1\\nline2"); // newline
    CHECK(escape_string("tab\there") == "tab\\there");       // tab
}

// -----------------------------------------------------------------------------
// serialize: Value tree -> JSON text. For scalars and single values the
// string comparison is deterministic.
// -----------------------------------------------------------------------------
TEST_CASE("serialize of scalars")
{
    Value s;
    s.data.emplace<std::string>("hello");
    CHECK(serialize(s) == "\"hello\"");

    Value yes;
    yes.data.emplace<bool>(true);
    CHECK(serialize(yes) == "true");

    Value no;
    no.data.emplace<bool>(false);
    CHECK(serialize(no) == "false");

    Value none;
    none.data.emplace<std::nullptr_t>(nullptr);
    CHECK(serialize(none) == "null");
}

TEST_CASE("serialize of numbers uses the minimal form")
{
    Value n;
    n.data.emplace<double>(42.0);
    CHECK(serialize(n) == "42"); // not "42.000000"

    Value neg;
    neg.data.emplace<double>(-2.5);
    CHECK(serialize(neg) == "-2.5");
}

TEST_CASE("serialize of arrays")
{
    CHECK(serialize(parse_json("[]")) == "[]");
    CHECK(serialize(parse_json("[1, 2, 3]")) == "[1, 2, 3]");
    CHECK(serialize(parse_json("[1, [2, 3]]")) == "[1, [2, 3]]");
    CHECK(serialize(parse_json("[true, \"x\", null]")) == "[true, \"x\", null]");
}

TEST_CASE("serialize of a single-key object")
{
    // A single key: ordering is not an issue.
    CHECK(serialize(parse_json("{\"a\": 1}")) == "{\"a\": 1}");
}

// -----------------------------------------------------------------------------
// serialize(value, {.pretty = true}): newlines + 2-space indent per level.
// We use single-key / index-only shapes so the output is deterministic.
// -----------------------------------------------------------------------------
TEST_CASE("serialize pretty-prints nested structures")
{
    // Scalars are unchanged (no newline).
    CHECK(serialize(parse_json("42"), {.pretty = true}) == "42");

    // Empty containers stay on one line.
    CHECK(serialize(parse_json("[]"), {.pretty = true}) == "[]");
    CHECK(serialize(parse_json("{}"), {.pretty = true}) == "{}");

    // A flat array: one element per line, 2-space indent, closing ] at depth 0.
    CHECK(serialize(parse_json("[1, 2]"), {.pretty = true}) == "[\n  1,\n  2\n]");

    // A single-key object holding an array: nested indentation grows.
    CHECK(serialize(parse_json("{\"a\": [1]}"), {.pretty = true}) ==
          "{\n  \"a\": [\n    1\n  ]\n}");
}

TEST_CASE("serialize honours a custom indent unit")
{
    // Four-space indentation.
    CHECK(serialize(parse_json("[1, 2]"), {.pretty = true, .indent_unit = "    "}) == "[\n    1,\n    2\n]");

    // Tab indentation, nesting grows one tab per level.
    CHECK(serialize(parse_json("{\"a\": [1]}"), {.pretty = true, .indent_unit = "\t"}) ==
          "{\n\t\"a\": [\n\t\t1\n\t]\n}");

    // Zero-width indent: newlines but no indentation.
    CHECK(serialize(parse_json("[1, 2]"), {.pretty = true, .indent_unit = ""}) == "[\n1,\n2\n]");
}

TEST_CASE("serialize can sort object keys")
{
    // Keys are emitted in ascending order, recursively into nested objects.
    Value v = parse_json("{\"zebra\": 1, \"apple\": {\"delta\": 4, \"beta\": 2}}");
    CHECK(serialize(v, {.sort_keys = true}) ==
          "{\"apple\": {\"beta\": 2, \"delta\": 4}, \"zebra\": 1}");

    // Without sorting, insertion order is preserved.
    CHECK(serialize(v) == "{\"zebra\": 1, \"apple\": {\"delta\": 4, \"beta\": 2}}");
}

TEST_CASE("@format segments format the output")
{
    // base64 encoding matches RFC 4648, including padding.
    CHECK(base64_encode("") == "");
    CHECK(base64_encode("a,b") == "YSxi");
    CHECK(base64_encode("hello") == "aGVsbG8=");

    // @base64 applied through the pipe.
    std::vector<Value> b = query_pipe(parse_json("\"a,b\""), "@base64");
    REQUIRE(b.size() == 1);
    CHECK(b[0].as_string() == "YSxi");

    // @csv quotes strings, doubles inner quotes, and blanks null.
    std::vector<Value> c = query_pipe(parse_json("[\"anna\", 30, true, null]"), "@csv");
    REQUIRE(c.size() == 1);
    CHECK(c[0].as_string() == "\"anna\",30,true,");

    // @tsv escapes tabs in fields and separates with a real tab.
    std::vector<Value> t = query_pipe(parse_json("[\"a\\tb\", \"c\"]"), "@tsv");
    REQUIRE(t.size() == 1);
    CHECK(t[0].as_string() == "a\\tb\tc");

    // @json serializes the value to a JSON string; @text is the raw form.
    CHECK(query_pipe(parse_json("{\"a\": 1}"), "@json")[0].as_string() == "{\"a\": 1}");
    CHECK(query_pipe(parse_json("\"hi\""), "@text")[0].as_string() == "hi");

    // @csv/@tsv need an array; unknown formats are rejected.
    CHECK_THROWS_AS(query_pipe(parse_json("42"), "@csv"), std::exception);
    CHECK_THROWS_AS(query_pipe(parse_json("1"), "@nope"), std::exception);
}

TEST_CASE("@base64d decodes and round-trips with @base64")
{
    CHECK(base64_decode("") == "");
    CHECK(base64_decode("YQ==") == "a");
    CHECK(base64_decode("YWI=") == "ab");
    CHECK(base64_decode("aGVsbG8=") == "hello");

    // Whitespace is ignored; invalid characters are rejected.
    CHECK(base64_decode("aGVs\nbG8=") == "hello");
    CHECK_THROWS_AS(base64_decode("!!!"), std::exception);

    // Encoding then decoding is the identity, via the pipe.
    std::vector<Value> r = query_pipe(parse_json("\"a,b\""), "@base64 | @base64d");
    REQUIRE(r.size() == 1);
    CHECK(r[0].as_string() == "a,b");
}

TEST_CASE("@uri percent-encodes reserved characters")
{
    CHECK(query_pipe(parse_json("\"hello world\""), "@uri")[0].as_string() ==
          "hello%20world");
    // Unreserved characters pass through unchanged.
    CHECK(query_pipe(parse_json("\"aZ0-_.~\""), "@uri")[0].as_string() == "aZ0-_.~");
    // '/' and '?' are reserved and get encoded.
    CHECK(query_pipe(parse_json("\"a/b?c\""), "@uri")[0].as_string() == "a%2Fb%3Fc");
}

TEST_CASE("@sh quotes values for the shell")
{
    // Strings are single-quoted, inner quotes escaped as '\''.
    CHECK(query_pipe(parse_json("\"it's a test\""), "@sh")[0].as_string() ==
          "'it'\\''s a test'");
    // Numbers and bools are emitted bare.
    CHECK(query_pipe(parse_json("42"), "@sh")[0].as_string() == "42");
    // An array escapes each element and joins with spaces.
    CHECK(query_pipe(parse_json("[\"a b\", 42, true]"), "@sh")[0].as_string() ==
          "'a b' 42 true");
    // null cannot be escaped for the shell.
    CHECK_THROWS_AS(query_pipe(parse_json("null"), "@sh"), std::exception);
}

// -----------------------------------------------------------------------------
// Round-trip: parse(serialize(parse(x))) must keep the same fields.
// -----------------------------------------------------------------------------
TEST_CASE("round-trip of a multi-key object")
{
    Value v = parse_json("{\"a\": 1, \"b\": [1, 2], \"c\": null}");
    std::string text = serialize(v);   // keys stay in insertion order
    Value reparsed = parse_json(text); // and it must be valid JSON

    CHECK(serialize(query(reparsed, "a")) == "1");
    CHECK(serialize(query(reparsed, "b")) == "[1, 2]");
    CHECK(serialize(query(reparsed, "c")) == "null");
}

// -----------------------------------------------------------------------------
// Objects keep the insertion order of their keys, so serialization is
// deterministic and mirrors the source document.
// -----------------------------------------------------------------------------
TEST_CASE("serialize preserves object key order")
{
    Value v = parse_json("{\"zebra\": 1, \"apple\": 2, \"mango\": 3}");
    CHECK(serialize(v) == "{\"zebra\": 1, \"apple\": 2, \"mango\": 3}");
}

// A duplicate key keeps the last value at the original position.
TEST_CASE("duplicate object keys keep the last value")
{
    Value v = parse_json("{\"a\": 1, \"b\": 2, \"a\": 9}");
    CHECK(serialize(v) == "{\"a\": 9, \"b\": 2}");
}

// -----------------------------------------------------------------------------
// tokenize: from text to the token list.
// -----------------------------------------------------------------------------
TEST_CASE("tokenize of punctuation")
{
    std::vector<Token> t = tokenize("{}[],:");
    REQUIRE(t.size() == 7); // 6 symbols + EndOfInput
    CHECK(t[0].type == TokenType::LBrace);
    CHECK(t[1].type == TokenType::RBrace);
    CHECK(t[2].type == TokenType::LBracket);
    CHECK(t[3].type == TokenType::RBracket);
    CHECK(t[4].type == TokenType::Comma);
    CHECK(t[5].type == TokenType::Colon);
    CHECK(t[6].type == TokenType::EndOfInput);
}

TEST_CASE("tokenize of strings, numbers and keywords")
{
    std::vector<Token> t = tokenize("\"hello\" 42 true false null");
    REQUIRE(t.size() == 6); // 5 values + EndOfInput
    CHECK(t[0].type == TokenType::String);
    CHECK(std::get<std::string>(t[0].value) == "hello");
    CHECK(t[1].type == TokenType::Number);
    CHECK(std::get<double>(t[1].value) == doctest::Approx(42.0));
    CHECK(t[2].type == TokenType::True);
    CHECK(t[3].type == TokenType::False);
    CHECK(t[4].type == TokenType::Null);
}

TEST_CASE("tokenize decodes escapes inside strings")
{
    std::vector<Token> t = tokenize("\"a\\nb\"");
    CHECK(std::get<std::string>(t[0].value) == "a\nb"); // \n -> real newline
}

// -----------------------------------------------------------------------------
// query / split_path / query_path
// -----------------------------------------------------------------------------
TEST_CASE("split_path splits on dots")
{
    std::vector<PathStep> steps = split_path(".a.b.c");
    REQUIRE(steps.size() == 3);
    CHECK(std::get<std::string>(steps[0]) == "a");
    CHECK(std::get<std::string>(steps[1]) == "b");
    CHECK(std::get<std::string>(steps[2]) == "c");

    CHECK(split_path("").empty());
    CHECK(split_path(".").empty()); // separators only -> no keys
}

TEST_CASE("split_path recognises array indices")
{
    std::vector<PathStep> steps = split_path(".users[1].name");
    REQUIRE(steps.size() == 3);
    CHECK(std::get<std::string>(steps[0]) == "users");
    CHECK(std::get<std::size_t>(steps[1]) == 1);
    CHECK(std::get<std::string>(steps[2]) == "name");

    // Consecutive indices: matrix[1][0]
    std::vector<PathStep> nested = split_path(".matrix[1][0]");
    REQUIRE(nested.size() == 3);
    CHECK(std::get<std::string>(nested[0]) == "matrix");
    CHECK(std::get<std::size_t>(nested[1]) == 1);
    CHECK(std::get<std::size_t>(nested[2]) == 0);
}

TEST_CASE("query gets a single field")
{
    Value v = parse_json("{\"name\": \"jpick\"}");
    CHECK(serialize(query(v, "name")) == "\"jpick\"");
}

TEST_CASE("query_path walks nested paths")
{
    Value v = parse_json("{\"a\": {\"b\": {\"c\": 42}}}");
    Value result = query_path(v, split_path(".a.b.c"))[0];
    CHECK(serialize(result) == "42");

    Value branch = query_path(v, split_path(".a.b"))[0];
    CHECK(serialize(branch) == "{\"c\": 42}");
}

TEST_CASE("query_index and query_path handle array indices")
{
    Value v = parse_json("{\"users\": [\"anna\", \"luca\", \"sara\"]}");
    CHECK(serialize(query_path(v, split_path(".users[1]"))[0]) == "\"luca\"");

    Value m = parse_json("{\"matrix\": [[1, 2], [3, 4]]}");
    CHECK(serialize(query_path(m, split_path(".matrix[1][0]"))[0]) == "3");

    // query_index directly
    Value arr = parse_json("[10, 20, 30]");
    CHECK(serialize(query_index(arr, 2)) == "30");
}

TEST_CASE("query_path iterates arrays with []")
{
    // .users[] expands to every element of the array.
    Value v = parse_json("{\"users\": [\"anna\", \"luca\", \"sara\"]}");
    std::vector<Value> all = query_path(v, split_path(".users[]"));
    REQUIRE(all.size() == 3);
    CHECK(all[0] == Value("anna"));
    CHECK(all[1] == Value("luca"));
    CHECK(all[2] == Value("sara"));

    // .users[].name applies the following step to each element.
    Value people = parse_json("{\"users\": [{\"name\": \"anna\"}, {\"name\": \"luca\"}]}");
    std::vector<Value> names = query_path(people, split_path(".users[].name"));
    REQUIRE(names.size() == 2);
    CHECK(names[0] == Value("anna"));
    CHECK(names[1] == Value("luca"));

    // [] on a non-array is an error.
    Value scalar = parse_json("42");
    CHECK_THROWS_AS(query_path(scalar, split_path(".[]")), std::exception);
}

TEST_CASE("query_index behavior: null for out-of-range, error for wrong type")
{
    Value arr = parse_json("[1, 2]");
    // Out of range returns null (jq-compatible).
    CHECK(query_index(arr, 5).is_null());

    Value obj = parse_json("{\"a\": 1}");
    // Wrong type (not an array) still throws.
    CHECK_THROWS_AS(query_index(obj, 0), std::exception);
}

// -----------------------------------------------------------------------------
// Error cases: functions must throw exceptions on invalid input.
// -----------------------------------------------------------------------------
TEST_CASE("errors: malformed input and missing fields")
{
    // Parser: missing value after the colon.
    CHECK_THROWS_AS(parse_json("{\"a\":}"), std::exception);

    // query: field that does not exist returns null (jq-compatible).
    Value v = parse_json("{\"a\": 1}");
    CHECK(query(v, "x").is_null());

    // query: the value is not an object still throws.
    Value number = parse_json("42");
    CHECK_THROWS_AS(query(number, "a"), std::exception);
}

// -----------------------------------------------------------------------------
// Value API: convenience constructors, type queries, typed accessors,
// operator[] navigation and equality.
// -----------------------------------------------------------------------------
TEST_CASE("Value convenience constructors and accessors")
{
    Value n = 42.0;    // double
    Value s = "jpick"; // const char* -> string (not bool!)
    Value b = true;
    Value nothing = nullptr;

    CHECK(n.is_number());
    CHECK(s.is_string());
    CHECK(b.is_bool());
    CHECK(nothing.is_null());

    CHECK(n.as_number() == doctest::Approx(42.0));
    CHECK(s.as_string() == "jpick");
    CHECK(b.as_bool() == true);

    // Wrong-type access throws.
    CHECK_THROWS_AS(n.as_string(), std::exception);
}

TEST_CASE("Value operator[] navigates objects and arrays")
{
    Value v = parse_json("{\"a\": {\"b\": [10, 20, 30]}}");
    CHECK(v["a"]["b"][2].as_number() == doctest::Approx(30.0));

    // Missing field and out-of-range index return null (jq-compatible).
    CHECK(v["missing"].is_null());
    CHECK(v["a"]["b"][5].is_null());
}

TEST_CASE("Value equality is order-independent for objects")
{
    // Same structure, keys written in a different order -> equal.
    Value a = parse_json("{\"x\": 1, \"y\": [2, 3]}");
    Value b = parse_json("{\"y\": [2, 3], \"x\": 1}");
    CHECK(a == b);

    // Different values -> not equal.
    Value c = parse_json("{\"x\": 1, \"y\": [2, 4]}");
    CHECK_FALSE(a == c);
}

// -----------------------------------------------------------------------------
// A full document is a single value: anything after it is an error.
// -----------------------------------------------------------------------------
TEST_CASE("parser rejects trailing content after a value")
{
    CHECK_THROWS_AS(parse_json("[1][2]"), std::exception);
    CHECK_THROWS_AS(parse_json("{\"a\": 1} {\"b\": 2}"), std::exception);
    CHECK_THROWS_AS(parse_json("1 2"), std::exception);
}

// -----------------------------------------------------------------------------
// parse_all: a stream of top-level values (NDJSON / whitespace-separated).
// -----------------------------------------------------------------------------
TEST_CASE("parse_all reads every top-level value")
{
    // Newline-delimited JSON. Keep the token vectors alive: Parser holds a
    // reference to them.
    std::vector<Token> ndjson_tokens = tokenize("{\"a\":1}\n{\"a\":2}\n{\"a\":3}");
    Parser ndjson(ndjson_tokens);
    std::vector<Value> values = ndjson.parse_all();
    REQUIRE(values.size() == 3);
    CHECK(values[0] == parse_json("{\"a\":1}"));
    CHECK(values[2] == parse_json("{\"a\":3}"));

    // Whitespace-separated scalars.
    std::vector<Token> scalar_tokens = tokenize("1 2 3");
    Parser scalars(scalar_tokens);
    CHECK(scalars.parse_all().size() == 3);

    // Empty input yields no values.
    std::vector<Token> empty_tokens = tokenize("");
    Parser empty(empty_tokens);
    CHECK(empty.parse_all().empty());
}

// -----------------------------------------------------------------------------
// Pipe: compose stages with `|`, applying each to every value of the stream.
// -----------------------------------------------------------------------------
TEST_CASE("split_pipe splits and trims segments")
{
    std::vector<std::string> segs = split_pipe(".a | .b | .c");
    REQUIRE(segs.size() == 3);
    CHECK(segs[0] == ".a");
    CHECK(segs[1] == ".b");
    CHECK(segs[2] == ".c");

    // No pipe -> a single segment.
    std::vector<std::string> one = split_pipe(".a.b");
    REQUIRE(one.size() == 1);
    CHECK(one[0] == ".a.b");
}

TEST_CASE("query_pipe composes stages")
{
    Value v = parse_json("{\"users\": [{\"name\": \"anna\"}, {\"name\": \"luca\"}]}");

    // .users[] | .name is equivalent to .users[].name
    std::vector<Value> names = query_pipe(v, ".users[] | .name");
    REQUIRE(names.size() == 2);
    CHECK(names[0] == Value("anna"));
    CHECK(names[1] == Value("luca"));

    // A plain path (no pipe) still works.
    std::vector<Value> single = query_pipe(v, ".users[0].name");
    REQUIRE(single.size() == 1);
    CHECK(single[0] == Value("anna"));

    // An empty expression selects the whole document.
    std::vector<Value> whole = query_pipe(v, "");
    REQUIRE(whole.size() == 1);
    CHECK(whole[0] == v);
}

// -----------------------------------------------------------------------------
// String interpolation: a "..." segment builds a string, replacing each
// \( ... ) with the value it produces.
// -----------------------------------------------------------------------------
TEST_CASE("split_pipe does not split inside string literals")
{
    // A '|' inside a string literal must not split the expression.
    std::vector<std::string> segs = split_pipe(".a | \"x|y\"");
    REQUIRE(segs.size() == 2);
    CHECK(segs[0] == ".a");
    CHECK(segs[1] == "\"x|y\"");

    // An escaped quote does not toggle the string state.
    std::vector<std::string> one = split_pipe("\"a\\\"|b\"");
    REQUIRE(one.size() == 1);
    CHECK(one[0] == "\"a\\\"|b\"");
}

TEST_CASE("raw_value emits strings without quotes and serializes the rest")
{
    CHECK(raw_value(Value("hello")) == "hello");
    CHECK(raw_value(Value(42.0)) == "42");
    CHECK(raw_value(Value(true)) == "true");
}

TEST_CASE("interpolate replaces \\( ... ) with the raw value")
{
    Value v = parse_json("{\"name\": \"anna\", \"age\": 30}");

    CHECK(interpolate("\"\\(.name): \\(.age)\"", v) == "anna: 30");

    // A literal with no interpolation is returned as-is.
    CHECK(interpolate("\"plain text\"", v) == "plain text");

    // Escapes are decoded.
    CHECK(interpolate("\"a\\nb\"", v) == "a\nb");
}

TEST_CASE("interpolate errors on multiple values or an unclosed \\(")
{
    Value v = parse_json("{\"a\": [1, 2, 3]}");

    // An inner expression that yields several values is rejected.
    CHECK_THROWS_AS(interpolate("\"\\(.a[])\"", v), std::exception);

    // A missing ')' is an error.
    CHECK_THROWS_AS(interpolate("\"\\(.a\"", v), std::exception);
}

TEST_CASE("query_pipe evaluates a string-literal stage")
{
    Value v = parse_json("{\"items\": [{\"n\": \"a\", \"c\": 1}, {\"n\": \"b\", \"c\": 2}]}");

    std::vector<Value> out = query_pipe(v, ".items[] | \"\\(.n)=\\(.c)\"");
    REQUIRE(out.size() == 2);
    CHECK(out[0] == Value("a=1"));
    CHECK(out[1] == Value("b=2"));
}

// -----------------------------------------------------------------------------
// Builtin functions: length, keys, type
// -----------------------------------------------------------------------------
TEST_CASE("builtin_length returns size for arrays, objects and strings")
{
    CHECK(builtin_length(parse_json("[1,2,3]")).as_number() == doctest::Approx(3.0));
    CHECK(builtin_length(parse_json("{\"a\":1,\"b\":2}")).as_number() == doctest::Approx(2.0));
    CHECK(builtin_length(parse_json("\"hello\"")).as_number() == doctest::Approx(5.0));

    // Other types return null.
    CHECK(builtin_length(parse_json("null")).is_null());
    CHECK(builtin_length(parse_json("42")).is_null());
    CHECK(builtin_length(parse_json("true")).is_null());
}

TEST_CASE("builtin_keys returns sorted keys for objects, indices for arrays")
{
    // Object: keys sorted alphabetically.
    Value obj_keys = builtin_keys(parse_json("{\"z\":1,\"a\":2,\"m\":3}"));
    CHECK(obj_keys == parse_json("[\"a\",\"m\",\"z\"]"));

    // Array: numeric indices.
    Value arr_keys = builtin_keys(parse_json("[10,20,30]"));
    CHECK(arr_keys == parse_json("[0,1,2]"));

    // Other types throw an error.
    CHECK_THROWS_AS(builtin_keys(parse_json("\"hello\"")), std::exception);
    CHECK_THROWS_AS(builtin_keys(parse_json("null")), std::exception);
}

TEST_CASE("builtin_type returns the JSON type as a string")
{
    CHECK(builtin_type(parse_json("null")).as_string() == "null");
    CHECK(builtin_type(parse_json("true")).as_string() == "boolean");
    CHECK(builtin_type(parse_json("42")).as_string() == "number");
    CHECK(builtin_type(parse_json("\"text\"")).as_string() == "string");
    CHECK(builtin_type(parse_json("[]")).as_string() == "array");
    CHECK(builtin_type(parse_json("{}")).as_string() == "object");
}

TEST_CASE("query_pipe recognizes builtin functions")
{
    Value v = parse_json("{\"users\":[{\"name\":\"anna\"},{\"name\":\"luca\"}]}");

    // length
    std::vector<Value> len = query_pipe(v, ".users | length");
    REQUIRE(len.size() == 1);
    CHECK(len[0].as_number() == doctest::Approx(2.0));

    // keys
    std::vector<Value> k = query_pipe(v, "keys");
    REQUIRE(k.size() == 1);
    CHECK(k[0] == parse_json("[\"users\"]"));

    // type
    std::vector<Value> t = query_pipe(v, ".users[0] | type");
    REQUIRE(t.size() == 1);
    CHECK(t[0].as_string() == "object");
}

// -----------------------------------------------------------------------------
// Builtin functions: not, empty, has
// -----------------------------------------------------------------------------
TEST_CASE("builtin_not negates booleans")
{
    CHECK(builtin_not(parse_json("true")).as_bool() == false);
    CHECK(builtin_not(parse_json("false")).as_bool() == true);

    // Non-booleans throw an error.
    CHECK_THROWS_AS(builtin_not(parse_json("null")), std::exception);
    CHECK_THROWS_AS(builtin_not(parse_json("42")), std::exception);
}

TEST_CASE("builtin_has tests object key presence")
{
    Value obj = parse_json("{\"a\": 1, \"b\": 2}");
    CHECK(builtin_has(obj, "a").as_bool() == true);
    CHECK(builtin_has(obj, "c").as_bool() == false);

    // Non-objects return false.
    CHECK(builtin_has(parse_json("[1,2,3]"), "a").as_bool() == false);
    CHECK(builtin_has(parse_json("null"), "a").as_bool() == false);
}

TEST_CASE("query_pipe handles has, not and empty")
{
    Value v = parse_json("{\"a\": 1}");

    // has("key")
    std::vector<Value> h1 = query_pipe(v, "has(\"a\")");
    REQUIRE(h1.size() == 1);
    CHECK(h1[0].as_bool() == true);

    std::vector<Value> h2 = query_pipe(v, "has(\"missing\")");
    REQUIRE(h2.size() == 1);
    CHECK(h2[0].as_bool() == false);

    // has combined with not
    std::vector<Value> hn = query_pipe(v, "has(\"missing\") | not");
    REQUIRE(hn.size() == 1);
    CHECK(hn[0].as_bool() == true);

    // empty removes everything from the stream.
    std::vector<Value> e = query_pipe(parse_json("[1,2,3]"), ".[] | empty");
    CHECK(e.empty());
}

// -----------------------------------------------------------------------------
// Array slicing: .[start:end], with negative and omitted bounds
// -----------------------------------------------------------------------------
TEST_CASE("split_path parses slice steps")
{
    std::vector<PathStep> steps = split_path(".[1:3]");
    REQUIRE(steps.size() == 1);
    const Slice *slice = std::get_if<Slice>(&steps[0]);
    REQUIRE(slice != nullptr);
    CHECK(slice->has_start);
    CHECK(slice->has_end);
    CHECK(slice->start == 1);
    CHECK(slice->end == 3);
}

TEST_CASE("query_path slices arrays")
{
    Value arr = parse_json("[0, 1, 2, 3, 4]");

    // [1:3] -> [1, 2]
    CHECK(query_path(arr, split_path(".[1:3]"))[0] == parse_json("[1, 2]"));

    // [:2] -> [0, 1]
    CHECK(query_path(arr, split_path(".[:2]"))[0] == parse_json("[0, 1]"));

    // [2:] -> [2, 3, 4]
    CHECK(query_path(arr, split_path(".[2:]"))[0] == parse_json("[2, 3, 4]"));

    // [-2:] -> [3, 4] (negative index counts from the end)
    CHECK(query_path(arr, split_path(".[-2:]"))[0] == parse_json("[3, 4]"));

    // [:-1] -> [0, 1, 2, 3]
    CHECK(query_path(arr, split_path(".[:-1]"))[0] == parse_json("[0, 1, 2, 3]"));

    // Out-of-range bounds are clamped, not an error.
    CHECK(query_path(arr, split_path(".[10:20]"))[0] == parse_json("[]"));
}

// -----------------------------------------------------------------------------
// Aggregate and array builtins: add, sort, unique, reverse, min, max,
// first, last, join, split
// -----------------------------------------------------------------------------
TEST_CASE("builtin_add sums, concatenates or merges")
{
    CHECK(builtin_add(parse_json("[1, 2, 3]")).as_number() == doctest::Approx(6.0));
    CHECK(builtin_add(parse_json("[\"a\", \"b\"]")).as_string() == "ab");
    CHECK(builtin_add(parse_json("[[1], [2, 3]]")) == parse_json("[1, 2, 3]"));
    CHECK(builtin_add(parse_json("[{\"a\":1}, {\"b\":2}]")) == parse_json("{\"a\":1,\"b\":2}"));

    // Later keys win when merging objects.
    CHECK(builtin_add(parse_json("[{\"a\":1}, {\"a\":2}]")) == parse_json("{\"a\":2}"));

    // Empty array yields null.
    CHECK(builtin_add(parse_json("[]")).is_null());
}

TEST_CASE("builtin_sort and builtin_unique order and dedupe")
{
    CHECK(builtin_sort(parse_json("[3, 1, 2]")) == parse_json("[1, 2, 3]"));
    CHECK(builtin_sort(parse_json("[\"b\", \"a\", \"c\"]")) == parse_json("[\"a\", \"b\", \"c\"]"));

    // Mixed types follow jq ordering: null < bool < number < string.
    CHECK(builtin_sort(parse_json("[\"x\", 1, null, true]")) ==
          parse_json("[null, true, 1, \"x\"]"));

    CHECK(builtin_unique(parse_json("[3, 1, 2, 1, 3]")) == parse_json("[1, 2, 3]"));
}

TEST_CASE("builtin_reverse reverses arrays and strings")
{
    CHECK(builtin_reverse(parse_json("[1, 2, 3]")) == parse_json("[3, 2, 1]"));
    CHECK(builtin_reverse(parse_json("\"abc\"")).as_string() == "cba");
}

TEST_CASE("builtin_min, builtin_max, builtin_first, builtin_last")
{
    Value arr = parse_json("[3, 1, 2]");
    CHECK(builtin_min(arr).as_number() == doctest::Approx(1.0));
    CHECK(builtin_max(arr).as_number() == doctest::Approx(3.0));
    CHECK(builtin_first(arr).as_number() == doctest::Approx(3.0));
    CHECK(builtin_last(arr).as_number() == doctest::Approx(2.0));

    // Empty array yields null for all of them.
    Value empty = parse_json("[]");
    CHECK(builtin_min(empty).is_null());
    CHECK(builtin_max(empty).is_null());
    CHECK(builtin_first(empty).is_null());
    CHECK(builtin_last(empty).is_null());
}

TEST_CASE("builtin_join and builtin_split")
{
    CHECK(builtin_join(parse_json("[\"a\", \"b\", \"c\"]"), ", ").as_string() == "a, b, c");

    // null contributes nothing, numbers are stringified.
    CHECK(builtin_join(parse_json("[\"a\", null, 2]"), "-").as_string() == "a--2");

    CHECK(builtin_split(parse_json("\"a,b,c\""), ",") == parse_json("[\"a\", \"b\", \"c\"]"));

    // A separator not present yields a single-element array.
    CHECK(builtin_split(parse_json("\"abc\""), ",") == parse_json("[\"abc\"]"));

    // An empty separator is an error.
    CHECK_THROWS_AS(builtin_split(parse_json("\"abc\""), ""), std::exception);
}

TEST_CASE("query_pipe dispatches the new builtins")
{
    CHECK(query_pipe(parse_json("{\"nums\":[3,1,2]}"), ".nums | sort")[0] ==
          parse_json("[1, 2, 3]"));
    CHECK(query_pipe(parse_json("{\"nums\":[1,2,3]}"), ".nums | add")[0].as_number() ==
          doctest::Approx(6.0));
    CHECK(query_pipe(parse_json("{\"tags\":[\"x\",\"y\"]}"), ".tags | join(\"-\")")[0].as_string() ==
          "x-y");
    CHECK(query_pipe(parse_json("{\"csv\":\"a,b\"}"), ".csv | split(\",\")")[0] ==
          parse_json("[\"a\", \"b\"]"));
}

// -----------------------------------------------------------------------------
// Alternative operator // and scalar literals
// -----------------------------------------------------------------------------
TEST_CASE("split_alternative splits on top-level // only")
{
    std::vector<std::string> parts = split_alternative(".a // .b // 0");
    REQUIRE(parts.size() == 3);
    CHECK(parts[0] == ".a");
    CHECK(parts[1] == ".b");
    CHECK(parts[2] == "0");

    // A // inside a string literal does not split.
    std::vector<std::string> one = split_alternative("\"a // b\"");
    REQUIRE(one.size() == 1);
    CHECK(one[0] == "\"a // b\"");
}

TEST_CASE("try_scalar_literal recognizes JSON scalars")
{
    Value out;
    CHECK(try_scalar_literal("true", out));
    CHECK(out.as_bool() == true);
    CHECK(try_scalar_literal("false", out));
    CHECK(out.as_bool() == false);
    CHECK(try_scalar_literal("null", out));
    CHECK(out.is_null());
    CHECK(try_scalar_literal("42", out));
    CHECK(out.as_number() == doctest::Approx(42.0));
    CHECK(try_scalar_literal("-3.5", out));
    CHECK(out.as_number() == doctest::Approx(-3.5));

    // Paths and words are not literals.
    CHECK_FALSE(try_scalar_literal(".a", out));
    CHECK_FALSE(try_scalar_literal("length", out));
    CHECK_FALSE(try_scalar_literal("12abc", out));
}

TEST_CASE("query_pipe applies the alternative operator //")
{
    // null and missing fall back; false falls back too.
    CHECK(query_pipe(parse_json("{\"a\":null}"), ".a // 0")[0].as_number() == doctest::Approx(0.0));
    CHECK(query_pipe(parse_json("{}"), ".x // 42")[0].as_number() == doctest::Approx(42.0));
    CHECK(query_pipe(parse_json("{\"a\":false}"), ".a // true")[0].as_bool() == true);

    // A present, truthy value wins; 0 is truthy.
    CHECK(query_pipe(parse_json("{\"a\":5}"), ".a // 9")[0].as_number() == doctest::Approx(5.0));
    CHECK(query_pipe(parse_json("{\"a\":0}"), ".a // 9")[0].as_number() == doctest::Approx(0.0));

    // Chained alternatives pick the first present value.
    CHECK(query_pipe(parse_json("{\"b\":7}"), ".a // .b // 0")[0].as_number() == doctest::Approx(7.0));

    // An error in an earlier alternative is ignored in favor of the fallback.
    CHECK(query_pipe(parse_json("42"), ".foo // 99")[0].as_number() == doctest::Approx(99.0));
}

// -----------------------------------------------------------------------------
// select(expr): keep values for which expr is truthy
// -----------------------------------------------------------------------------
TEST_CASE("query_pipe filters with select")
{
    Value v = parse_json("{\"users\":[{\"name\":\"anna\",\"active\":true},"
                         "{\"name\":\"luca\",\"active\":false},"
                         "{\"name\":\"sara\",\"active\":true}]}");

    // Keep only active users, then take their names.
    std::vector<Value> names = query_pipe(v, ".users[] | select(.active) | .name");
    REQUIRE(names.size() == 2);
    CHECK(names[0] == Value("anna"));
    CHECK(names[1] == Value("sara"));

    // A missing field is falsy, not an error: it just filters the value out.
    std::vector<Value> some = query_pipe(parse_json("[{\"active\":true},{\"x\":1}]"),
                                         ".[] | select(.active)");
    REQUIRE(some.size() == 1);
    CHECK(some[0] == parse_json("{\"active\":true}"));

    // select with has() as the predicate.
    std::vector<Value> withA = query_pipe(parse_json("[{\"a\":1},{\"b\":2},{\"a\":3}]"),
                                          ".[] | select(has(\"a\")) | .a");
    REQUIRE(withA.size() == 2);
    CHECK(withA[0].as_number() == doctest::Approx(1.0));
    CHECK(withA[1].as_number() == doctest::Approx(3.0));

    // No match yields an empty stream.
    std::vector<Value> none = query_pipe(parse_json("[{\"active\":false}]"),
                                         ".[] | select(.active)");
    CHECK(none.empty());
}

TEST_CASE("split_pipe and split_alternative are paren-aware")
{
    // A '|' inside parentheses does not split the pipe.
    std::vector<std::string> segs = split_pipe(".a | select(.b | .c)");
    REQUIRE(segs.size() == 2);
    CHECK(segs[0] == ".a");
    CHECK(segs[1] == "select(.b | .c)");

    // A '//' inside parentheses does not split the alternative.
    std::vector<std::string> parts = split_alternative("select(.n // 0)");
    REQUIRE(parts.size() == 1);
    CHECK(parts[0] == "select(.n // 0)");
}
