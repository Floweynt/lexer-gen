#include "argparse.h"
#include "build_config.h"
#include "machine/dfa.h"
#include "machine/nfa.h"
#include "regex.h"
#include "utils.h"
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
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

    auto format_bitmask(std::vector<bool> bitset) -> std::string
    {
        std::string buf;
        for (size_t i = 0; i < bitset.size(); i++)
        {
            if (bitset[i])
            {
                buf += std::to_string(i);
                buf.push_back(',');
            }
        }
        buf.pop_back();
        return buf;
    }
} // namespace

inline static constexpr lexergen::option options[] = {
    {
        .name = "dfa-out",
        .long_flag = "--emit-dfa-dot",
        .short_flag = "-D",
        .description = "specifies the output for the dot file for DFA graph visualization",
        .has_args = true,
        .required = false,
    },
    {
        .name = "nfa-out",
        .long_flag = "--emit-nfa-dot",
        .short_flag = "-N",
        .description = "specifies the output for the dot file for NFA graph visualization",
        .has_args = true,
        .required = false,
    },
    {
        .name = "cpp-out",
        .long_flag = "--output",
        .short_flag = "-o",
        .description = "specifies the output source file",
        .has_args = true,
        .required = true,
    },
    //    {"alignment", "--align", "-a", "specifies that equivalence classes should be padded to 2^n, or next 2^n if set to auto", true, false},
    //   {"type", "--type", "-t", "enables smallest int type selection", false, false},
    {
        .name = "debug",
        .long_flag = "--debug",
        .short_flag = "-d",
        .description = "print some internal information",
        .has_args = false,
        .required = false,
    },
    {
        .name = "equivalence-class",
        .long_flag = "--equivalence-class",
        .short_flag = "-c",
        .description = "enables equivalence classes, which usually results in a DFA smaller table",
        .has_args = false,
        .required = false,
    },
    {
        .name = "optimize",
        .long_flag = "--optimize",
        .short_flag = "-O",
        .description = "enables DFA optimization via minimization",
        .has_args = false,
        .required = false,
    },
};

auto main(int argc, const char* argv[]) -> int
{
    auto [files, args] = lexergen::parse_args(
        std::span<const char*>(argv + 1, argc - 1),
        lexergen::arg_spec{
            .options = options,
            .program_name = "lexer-gen",
            .version = "v" VERSION,
        }
    );

    if (files.size() != 1)
    {
        std::cerr << "expected only one input file\n";
        exit(-1);
    }

    std::ifstream in_file(files[0]);

    // The file format is defined as:
    // [preamble]
    // %%
    // RULE /expr/ [handler]
    // UNKNOWN [handler]
    // ERROR [handler]
    // MACRO name /expr/
    // CLASS [class]

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

        auto [success, expr, error_msg, rest] = lexergen::parse_regex(line);

        if (!success)
        {
            std::cerr << std::format("failed to parse line `{}`: {}\n", line, error_msg);
            exit(-1);
        }

        tokens.emplace_back(expr, rest);
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

    if (args["optimize"].present)
    {
        dfa.optimize(args["debug"].present);
    }

    if (args["dfa-out"].present)
    {
        std::ofstream dot_out(args["dfa-out"].value);
        if (!dot_out)
        {
            std::cerr << "unable to open file: " << args["dfa-out"].value << '\n';
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
        std::cout << std::format("transition table (rle): ");
        const auto& tab = res.transition;
        size_t rle_count = 0;
        auto rle_curr = tab[0];

        for (auto ent : tab)
        {
            if (ent != rle_curr)
            {
                std::cout << std::format("({}, {}), ", rle_curr, rle_count);
                rle_count = 1;
                rle_curr = ent;
            }
            else
            {
                rle_count++;
            }
        }

        std::cout << std::format("({}, {})\n", rle_curr, rle_count);
        std::cout << std::format("transition table takes: {} i64 = {} bytes\n", tab.size(), tab.size() * sizeof(int64_t));
        std::cout << std::format("start state: {}\n", dfa.get_start_state());
        std::cout << std::format("bitmask {}\n", format_bitmask(dfa.get_end_bitmask()));
        std::cout << std::format("states {}\n", dfa.get_end_bitmask().size());
        std::cout << std::format("state mapping {}\n", lexergen::format_table(dfa.get_end_to_nfa_state()));

        if (args["equivalence-class"].present)
        {
            std::cout << std::format("classifier: {}\n", lexergen::format_table(res.classifier));
            std::cout << std::format("classes: {}\n", res.class_count);
        }
    }
}
