#include <iostream>
#include <fstream>
#include <string>
#include "CLI11.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "query.hpp"
#include "serializer.hpp"

int main(int argc, char *argv[])
{
    CLI::App app{"jpick - a tiny jq-like JSON tool"};

    std::string path;
    std::string file;
    bool pretty = false;

    app.add_option("path", path, "Query path, e.g. '.a.b[0]'");
    app.add_option("file", file, "JSON file to read (default: stdin)");
    app.add_flag("-p,--pretty", pretty, "Pretty-print the output");

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
        Value value = parser.parse_value();

        if (!path.empty())
        {
            std::vector<PathStep> keys = split_path(path);
            value = query_path(value, keys);
        }

        std::cout << serialize(value, pretty) << '\n';
    }
    catch (const std::exception &e)
    {
        std::cerr << "jpick: " << e.what() << '\n';
        return 1;
    }
    return 0;
}