// fuzz_parse.cpp — libFuzzer harness for the JSON front end (tokenize + parse).
//
// Goal: feed arbitrary bytes to tokenize() and the recursive-descent Parser,
// letting ASan/UBSan catch memory/UB errors and libFuzzer's stack guard catch
// the deep-nesting stack overflow in the recursive parser (e.g. "[[[[[[...").
//
// THE GOTCHA to internalise: jpick uses C++ exceptions for normal error
// handling — malformed JSON makes tokenize()/Parser::parse() THROW. If an
// exception escapes LLVMFuzzerTestOneInput it reaches std::terminate -> abort,
// which libFuzzer reports as a "crash" even though it is the intended error
// path. So you MUST catch std::exception here; only real UB, an ASan report, a
// stack overflow or a SEGV should count as a finding.
#include <cstddef>
#include <cstdint>
#include <string>

#include "jpick/lexer.hpp"
#include "jpick/parser.hpp"

using namespace jpick;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    (void)data;
    (void)size;
    std::string input(reinterpret_cast<const char *>(data), size);
    try
    {
        std::vector<Token> tokens = tokenize(input);
        Parser parser(tokens);
        parser.parse();
    }
    catch (const std::exception &)
    {
    }
    return 0;
}
