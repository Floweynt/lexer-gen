
#include "argparse.h"
#include "utils.h"
#include <bitset>
#include <cinttypes>
#include <fmt/ranges.h>
#include <fstream>
#include <iostream>
#include <list>
#include <machine/dfa.h>
#include <machine/nfa.h>
#include <queue>
#include <regex.h>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    auto get_str_section(std::istream& stream) -> std::string
    {
        std::string buf;
        std::string line;
        while (std::getline(stream, line) && line != "%%")
        {
            buf += line + '\n';
        }

        return buf;
    }
} // namespace

inline static constexpr lexergen::option options[] = {
    {"dot-out", "--emit-dfa-dot", "-D", "specifies the output for the dot file for DFA graph visualization", true, false},
    {"nfa-out", "--emit-nfa-dot", "-N", "specifies the output for the dot file for NFA graph visualization", true, false},
    {"cpp-out", "--output", "-o", "specifies the output source file", true, true},
};

auto main(int argc, const char* argv[]) -> int
{
    auto [files, args] = lexergen::parse_args(std::span<const char*>(argv + 1, argc - 1), lexergen::arg_spec{options, "lexer-gen", "v0.0.1"});

    if (files.size() != 1)
    {
        std::cerr << "expected only one input file\n";
        exit(-1);
    }

    std::ifstream in_file(files[0]);
    std::string preamble = get_str_section(in_file);
    std::string handle_eof = get_str_section(in_file);
    std::string handle_error = get_str_section(in_file);
    std::string handle_internal_error = get_str_section(in_file);
    std::vector<std::pair<lexergen::regex, std::string>> tokens;

    std::string line;
    while (std::getline(in_file, line))
    {
        if (line.empty() || line.starts_with(";") || line.starts_with("#"))
        {
            continue;
        }

        auto index = line.find(" -> ");
        std::string pattern = line.substr(0, index);
        std::string handler = line.substr(index + 4);
        lexergen::regex entry;

        if (pattern[0] == '"')
        {
            entry = lexergen::string_regex(lexergen::handle_escape_str(pattern.substr(1, pattern.size() - 2)));
        }
        else
        {
            lexergen::regex_parser parser(pattern);
            entry = parser.parse();
            parser.dump_error();
            if (parser.has_errors())
            {
                exit(-1);
            }
        }

        tokens.emplace_back(entry, handler);
    }

    std::ofstream out(args["cpp-out"].value);
    if (!out)
    {
        std::cerr << "unable to open file: " << args["cpp-out"].value << '\n';
        exit(-1);
    }

    auto [dfa, nfa] = lexergen::make_lexer(tokens);
    dfa.codegen(out, preamble, handle_eof, handle_error, handle_internal_error);

    if (args["dot-out"].present)
    {
        std::ofstream dot_out(args["dot-out"].value);
        if (!dot_out)
        {
            std::cerr << "unable to open file: " << args["dot-out"].value << '\n';
            exit(-1);
        }

        dfa.dump(dot_out);
    }

    if (args["nfa-out"].present)
    {
        std::ofstream dot_out(args["nfa-out"].value);
        if (!dot_out)
        {
            std::cerr << "unable to open file: " << args["nfa-out"].value << '\n';
            exit(-1);
        }

        nfa.dump(dot_out);
    }
}
