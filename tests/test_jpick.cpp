// Test suite for jpick.
//
// Uses doctest (third_party/doctest.h). The macro below tells doctest to
// generate the test program's main() for us: we only write the TEST_CASEs.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "json.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "query.hpp"
#include "serializer.hpp"

// Helper: from JSON text to the Value tree in one shot.
// Mirrors what main.cpp does: tokenize -> Parser -> parse_value.
static Value parse_json(const std::string &text)
{
    std::vector<Token> tokens = tokenize(text);
    Parser parser(tokens);
    return parser.parse_value();
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
// serialize(value, /*pretty=*/true): newlines + 2-space indent per level.
// We use single-key / index-only shapes so the output is deterministic.
// -----------------------------------------------------------------------------
TEST_CASE("serialize pretty-prints nested structures")
{
    // Scalars are unchanged (no newline).
    CHECK(serialize(parse_json("42"), true) == "42");

    // Empty containers stay on one line.
    CHECK(serialize(parse_json("[]"), true) == "[]");
    CHECK(serialize(parse_json("{}"), true) == "{}");

    // A flat array: one element per line, 2-space indent, closing ] at depth 0.
    CHECK(serialize(parse_json("[1, 2]"), true) == "[\n  1,\n  2\n]");

    // A single-key object holding an array: nested indentation grows.
    CHECK(serialize(parse_json("{\"a\": [1]}"), true) ==
          "{\n  \"a\": [\n    1\n  ]\n}");
}

// -----------------------------------------------------------------------------
// Round-trip: parse(serialize(parse(x))) must keep the same fields.
// With multi-key objects we do NOT compare strings (unordered_map does not
// guarantee ordering): we re-parse and query the fields.
// -----------------------------------------------------------------------------
TEST_CASE("round-trip of a multi-key object")
{
    Value v = parse_json("{\"a\": 1, \"b\": [1, 2], \"c\": null}");
    std::string text = serialize(v);   // key order unknown
    Value reparsed = parse_json(text); // but it must be valid JSON

    CHECK(serialize(query(reparsed, "a")) == "1");
    CHECK(serialize(query(reparsed, "b")) == "[1, 2]");
    CHECK(serialize(query(reparsed, "c")) == "null");
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
    Value result = query_path(v, split_path(".a.b.c"));
    CHECK(serialize(result) == "42");

    Value branch = query_path(v, split_path(".a.b"));
    CHECK(serialize(branch) == "{\"c\": 42}");
}

TEST_CASE("query_index and query_path handle array indices")
{
    Value v = parse_json("{\"users\": [\"anna\", \"luca\", \"sara\"]}");
    CHECK(serialize(query_path(v, split_path(".users[1]"))) == "\"luca\"");

    Value m = parse_json("{\"matrix\": [[1, 2], [3, 4]]}");
    CHECK(serialize(query_path(m, split_path(".matrix[1][0]"))) == "3");

    // query_index directly
    Value arr = parse_json("[10, 20, 30]");
    CHECK(serialize(query_index(arr, 2)) == "30");
}

TEST_CASE("query_index errors: out of range and non-array")
{
    Value arr = parse_json("[1, 2]");
    CHECK_THROWS_AS(query_index(arr, 5), std::exception); // out of range

    Value obj = parse_json("{\"a\": 1}");
    CHECK_THROWS_AS(query_index(obj, 0), std::exception); // not an array
}

// -----------------------------------------------------------------------------
// Error cases: functions must throw exceptions on invalid input.
// -----------------------------------------------------------------------------
TEST_CASE("errors: malformed input and missing fields")
{
    // Parser: missing value after the colon.
    CHECK_THROWS_AS(parse_json("{\"a\":}"), std::exception);

    // query: field that does not exist.
    Value v = parse_json("{\"a\": 1}");
    CHECK_THROWS_AS(query(v, "x"), std::exception);

    // query: the value is not an object.
    Value number = parse_json("42");
    CHECK_THROWS_AS(query(number, "a"), std::exception);
}
