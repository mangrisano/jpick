#include <iostream>
#include <fstream>
#include <string>
#include "CLI11.hpp"
#include "jpick/lexer.hpp"
#include "jpick/parser.hpp"
#include "jpick/query.hpp"
#include "jpick/serializer.hpp"

using namespace jpick;

int main(int argc, char *argv[])
{
    CLI::App app{"jpick - a tiny jq-like JSON tool"};
    app.set_version_flag("-v,--version", JPICK_VERSION);

    std::string path;
    std::string file;
    bool pretty = false;
    bool raw = false;

    app.add_option("path", path, "Query path, e.g. '.a.b[0]'");
    app.add_option("file", file, "JSON file to read (default: stdin)");
    app.add_flag("-p,--pretty", pretty, "Pretty-print the output");
    app.add_flag("-r,--raw-output", raw, "Output strings without quotes or escaping");

    CLI11_PARSE(app, argc, argv);

    std::string input;
    if (!file.empty())
    {
        std::ifstream stream(file);
        if (!stream)
        {
            std::cerr << "jpick: cannot open file: " << file << '\n';
            return 1;
        }
        input.assign(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
    }
    else
    {
        input.assign(std::istreambuf_iterator<char>(std::cin),
                     std::istreambuf_iterator<char>());
    }

    try
    {
        std::vector<Token> tokens = tokenize(input);
        Parser parser(tokens);
        Value value = parser.parse();

        std::vector<Value> results = query_pipe(value, path);

        for (const Value &result : results)
        {
            if (raw && result.is_string())
                std::cout << result.as_string() << '\n';
            else
                std::cout << serialize(result, pretty) << '\n';
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "jpick: " << e.what() << '\n';
        return 1;
    }
    return 0;
}