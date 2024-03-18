#include "argparse.h"
#include "fwd.h"
#include "utils.h"
#include <bitset>
#include <cinttypes>
#include <cstdint>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fstream>
#include <iostream>
#include <list>
#include <machine/dfa.h>
#include <machine/nfa.h>
#include <queue>
#include <ranges>
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
    //    {"alignment", "--align", "-a", "specifies that equivalence classes should be padded to 2^n, or next 2^n if set to auto", true, false},
    //   {"type", "--type", "-t", "enables smallest int type selection", false, false},
    {"debug", "--debug", "-d", "print some internal information", false, false},
    {"equivalence-class", "--equivalence-class", "-c", "enables equivalence classes, which usually results in a DFA smaller table", false, false},
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
    std::string handle_error = get_str_section(in_file);
    std::string handle_internal_error = get_str_section(in_file);
    std::vector<std::pair<lexergen::regex, std::string>> tokens;

    std::string line;
    while (std::getline(in_file, line))
    {
        if (line == "%%")
        {
            break;
        }

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

    std::string file_end = get_str_section(in_file);

    std::ofstream out(args["cpp-out"].value);
    if (!out)
    {
        std::cerr << "unable to open file: " << args["cpp-out"].value << '\n';
        exit(-1);
    }

    auto [dfa, nfa] = lexergen::make_lexer(tokens);
    auto res = dfa.codegen(out, preamble, handle_error, handle_internal_error, args["equivalence-class"].present);

    if (!file_end.empty())
    {
        out << file_end;
    }

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

    if (args["debug"].present)
    {
        fmt::print("transition table (rle): ");
        const auto& tab = res.transition;
        size_t rle_count = 0;
        auto rle_curr = tab[0];

        for (auto ent : tab)
        {
            if (ent != rle_curr)
            {
                fmt::print("({}, {}), ", rle_curr, rle_count);
                rle_count = 1;
                rle_curr = ent;
            }
            else
            {
                rle_count++;
            }
        }
        fmt::println("({}, {})", rle_curr, rle_count);
        fmt::println("transition table takes: {} i64 = {} bytes", tab.size(), tab.size() * sizeof(int64_t));
        fmt::println("start state: {}", dfa.get_start_state());
        fmt::println("bitmask {}", fmt::join(dfa.get_end_bitmask() | std::ranges::views::transform([](bool flag) { return (int)flag; }), ""));
        fmt::println("states {}", dfa.get_end_bitmask().size());
        fmt::println("state mapping {}", dfa.get_end_to_nfa_state());

        if (args["equivalence-class"].present)
        {
            fmt::println("classifier: {}", res.classifier);
            fmt::println("classes: {}", res.class_count);
        }
    }
}
