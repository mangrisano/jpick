// fuzz_query.cpp — libFuzzer harness for the query engine (query_pipe).
//
// Goal: exercise the query pipeline — split_on/split_pipe, eval_simple,
// interpolate, is_array_construction/is_group, the @formats, comparisons — with
// arbitrary query strings, against a small fixed JSON document.
//
// Same GOTCHA as fuzz_parse: query_pipe THROWS on invalid queries by design, so
// catch std::exception; only real UB / ASan / stack overflow / SEGV is a bug.
//
// Design note (decide when you write it): the simplest first version fuzzes the
// QUERY string only, against a constant JSON value. A richer version could
// carve the fuzz bytes into "query \0 json" so both sides get mutated.
#include <cstddef>
#include <cstdint>
#include <string>

#include "jpick/json.hpp"
#include "jpick/query.hpp"

using namespace jpick;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
    std::string query(reinterpret_cast<const char *>(data), size);
    static const Value doc = [] {
        auto tokens = tokenize(R"({"a":[1,2,{"b":3}],"s":"hi","n":42})");
        return Parser(tokens).parse();
    }();
    try {
        query_pipe(doc, query);
    }
    catch (const std::exception &) {}
    return 0;
}
