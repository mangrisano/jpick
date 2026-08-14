// fuzz_roundtrip.cpp — property fuzzer for the parse/serialize round-trip.
//
// Invariant under test: for any input that parses to a value v1,
//     parse(serialize(v1))  ==  v1
// i.e. serializing a value and parsing it back must yield an equal value.
// This catches serializer and number-formatting bugs the other harnesses miss.
//
// TWO THINGS that differ from fuzz_parse / fuzz_query:
//
//   1. A broken invariant is NOT an exception — libFuzzer only notices crashes
//      and sanitizer reports. So when v1 != v2 you must actively signal it,
//      e.g. abort()/__builtin_trap(), or the bug would pass silently.
//
//   2. Only the FIRST parse is allowed to throw (malformed input is expected
//      and not our concern -> catch and return). The SECOND parse, of
//      serialize(v1), must succeed: if serialize produced invalid JSON, letting
//      that exception escape is itself a real finding (a crash), so keep the
//      second parse OUT of the swallowing try.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "jpick/json.hpp"
#include "jpick/lexer.hpp"
#include "jpick/parser.hpp"
#include "jpick/serializer.hpp"

using namespace jpick;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::string input(reinterpret_cast<const char *>(data), size);
    Value v1;
    try
    {
        v1 = Parser(tokenize(input)).parse();
    }
    catch (const std::exception &)
    {
        return 0;
    }
    Value v2 = Parser(tokenize(serialize(v1))).parse();
    if (!(v1 == v2))
        abort();
    return 0;
}
