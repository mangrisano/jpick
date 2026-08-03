#include <iostream>
#include <fstream>
#include <string>
#include "CLI11.hpp"
#include "jpick/lexer.hpp"
#include "jpick/parser.hpp"
#include "jpick/query.hpp"
#include "jpick/serializer.hpp"

using namespace jpick;

// Split raw text into one string Value per line (used by --raw-input). Each
// line's trailing newline is dropped; a final newline does not yield an extra
// empty value.
static std::vector<Value> read_raw_lines(const std::string &input)
{
    std::vector<Value> lines;
    std::size_t start = 0;
    while (start < input.size())
    {
        std::size_t newline = input.find('\n', start);
        if (newline == std::string::npos)
        {
            lines.emplace_back(input.substr(start));
            break;
        }
        lines.emplace_back(input.substr(start, newline - start));
        start = newline + 1;
    }
    return lines;
}

int main(int argc, char *argv[])
{
    CLI::App app{"jpick - a tiny jq-like JSON tool"};
    app.set_version_flag("-v,--version", JPICK_VERSION);

    std::string path;
    std::string file;
    bool pretty = false;
    bool raw = false;
    bool tab = false;
    bool sort_keys = false;
    bool slurp = false;
    bool raw_input = false;
    int indent_spaces = -1;

    app.add_option("path", path, "Query path, e.g. '.a.b[0]'");
    app.add_option("file", file, "JSON file to read (default: stdin)");
    app.add_flag("-p,--pretty", pretty, "Pretty-print the output");
    app.add_flag("-r,--raw-output", raw, "Output strings without quotes or escaping");
    app.add_flag("-S,--sort-keys", sort_keys, "Sort object keys in the output");
    app.add_flag("-s,--slurp", slurp, "Read all inputs into a single array");
    app.add_flag("-R,--raw-input", raw_input,
                 "Read each input line as a string instead of parsing JSON");
    auto *indent_opt = app.add_option("--indent", indent_spaces,
                                      "Indent with N spaces (implies --pretty)")
                           ->check(CLI::NonNegativeNumber);
    app.add_flag("--tab", tab, "Indent with tabs (implies --pretty)")
        ->excludes(indent_opt);

    CLI11_PARSE(app, argc, argv);

    // Resolve the indentation unit. --tab or --indent imply pretty output.
    std::string indent_unit = "  ";
    if (tab)
    {
        indent_unit = "\t";
        pretty = true;
    }
    else if (indent_spaces >= 0)
    {
        indent_unit = std::string(static_cast<std::size_t>(indent_spaces), ' ');
        pretty = true;
    }

    SerializeOptions serialize_opts{pretty, indent_unit, sort_keys};

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
        std::vector<Value> inputs;
        if (raw_input)
        {
            // --raw-input treats the input as text, not JSON. With --slurp the
            // whole input becomes one string; otherwise each line becomes one.
            if (slurp)
                inputs = {Value(input)};
            else
                inputs = read_raw_lines(input);
        }
        else
        {
            std::vector<Token> tokens = tokenize(input);
            Parser parser(tokens);
            inputs = parser.parse_all();

            // --slurp collects every input value into a single array.
            if (slurp)
            {
                Array all = std::move(inputs);
                inputs = {Value(std::move(all))};
            }
        }

        for (const Value &input_value : inputs)
        {
            std::vector<Value> results = query_pipe(input_value, path);
            for (const Value &result : results)
            {
                if (raw && result.is_string())
                    std::cout << result.as_string() << '\n';
                else
                    std::cout << serialize(result, serialize_opts) << '\n';
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "jpick: " << e.what() << '\n';
        return 1;
    }
    return 0;
}