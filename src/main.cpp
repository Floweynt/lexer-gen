#include "argparse.h"
#include "build_config.h"
#include "machine/cg.h"
#include "machine/dfa.h"
#include "machine/nfa.h"
#include "regex.h"
#include "utils.h"
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    auto trim(std::string_view str) -> std::string_view
    {
        auto start = str.find_first_not_of(" \t\r");
        if (start == std::string_view::npos)
        {
            return {};
        }

        auto end = str.find_last_not_of(" \t\r");
        return str.substr(start, end - start + 1);
    }

    auto split_first_word(std::string_view str) -> std::pair<std::string_view, std::string_view>
    {
        auto ws = str.find_first_of(" \t");
        if (ws == std::string_view::npos)
        {
            return {str, {}};
        }

        auto word = str.substr(0, ws);
        auto rest_start = str.find_first_not_of(" \t", ws);
        auto rest = rest_start == std::string_view::npos ? std::string_view{} : str.substr(rest_start);
        return {word, rest};
    }

    auto get_str_section(std::istream& stream) -> std::string
    {
        std::string buf;
        std::string line;
        while (std::getline(stream, line) && trim(line) != "%%")
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
        .name = "optimize",
        .long_flag = "--optimize",
        .short_flag = "-O",
        .description = "enables DFA optimization via minimization",
        .has_args = false,
        .required = false,
    },
    {
        .name = "lang",
        .long_flag = "--lang",
        .short_flag = "-l",
        .description = "target language: cpp, c, java, javascript, python (default: inferred from -o's extension, else cpp)",
        .has_args = true,
        .required = false,
    },
};

auto main(int argc, const char* argv[]) -> int
{
    auto spec = lexergen::arg_spec{
        .options = options,
        .program_name = "lexer-gen",
        .version = "v" VERSION,
    };

    auto [files, args] = lexergen::parse_args(std::span<const char*>(argv + 1, argc - 1), spec);

    if (files.size() != 1)
    {
        std::cerr << "expected only one input file\n";
        exit(-1);
    }

    std::ifstream in_file(files[0]);
    if (!in_file)
    {
        std::cerr << "unable to open file: " << files[0] << '\n';
        exit(-1);
    }

    // The file format is defined as:
    // [preamble]
    // %%
    // RULE /expr/ [handler]
    // UNKNOWN [handler]
    // ERROR [handler]
    // MACRO name /expr/
    // %%
    // [epilogue]

    std::string preamble = get_str_section(in_file);
    std::string handle_error;
    std::string handle_internal_error;
    bool has_unknown = false;
    bool has_error = false;
    std::vector<std::pair<lexergen::regex, std::string>> tokens;
    lexergen::macro_table macros;

    std::string line;

    while (std::getline(in_file, line))
    {
        auto trimmed = trim(line);

        if (trimmed == "%%")
        {
            break;
        }

        if (trimmed.empty() || trimmed.starts_with(';') || trimmed.starts_with('#'))
        {
            continue;
        }

        auto [word, directive_rest] = split_first_word(trimmed);

        if (word == "MACRO")
        {
            auto [name, expr_str] = split_first_word(directive_rest);
            if (name.empty() || expr_str.empty())
            {
                std::cerr << std::format("malformed line `{}`: expected `MACRO name /expr/`\n", line);
                exit(-1);
            }

            auto [success, expr, error_msg, macro_rest] = lexergen::parse_regex(std::string(expr_str), macros);
            if (!success)
            {
                std::cerr << std::format("failed to parse macro `{}`: {}\n", name, error_msg);
                exit(-1);
            }

            macros[std::string(name)] = expr;
            continue;
        }

        if (word == "UNKNOWN")
        {
            handle_error = std::string(directive_rest);
            has_unknown = true;
            continue;
        }

        if (word == "ERROR")
        {
            handle_internal_error = std::string(directive_rest);
            has_error = true;
            continue;
        }

        auto rule_line = word == "RULE" ? directive_rest : trimmed;
        auto [success, expr, error_msg, handler] = lexergen::parse_regex(std::string(rule_line), macros);

        if (!success)
        {
            std::cerr << std::format("failed to parse line `{}`: {}\n", line, error_msg);
            exit(-1);
        }

        tokens.emplace_back(expr, handler);
    }

    if (!has_unknown || !has_error)
    {
        std::cerr << "missing UNKNOWN and/or ERROR handler directive\n";
        exit(-1);
    }

    std::string file_end = get_str_section(in_file);

    auto lang = lexergen::target_lang::CPP;
    if (args["lang"].present)
    {
        auto parsed = lexergen::parse_target_lang(args["lang"].value);
        if (!parsed)
        {
            std::cerr << std::format("unknown --lang '{}' (expected cpp, c, java, javascript, python)\n", args["lang"].value);
            exit(-1);
        }
        lang = *parsed;
    }
    else if (auto inferred = lexergen::infer_target_lang(args["cpp-out"].value))
    {
        lang = *inferred;
    }

    std::ofstream out(args["cpp-out"].value);
    if (!out)
    {
        std::cerr << "unable to open file: " << args["cpp-out"].value << '\n';
        exit(-1);
    }

    auto [dfa, nfa] = lexergen::make_lexer(tokens);

    if (args["optimize"].present)
    {
        dfa.optimize(args["debug"].present);
    }

    auto res = dfa.codegen(out, preamble, handle_error, handle_internal_error, lang);

    if (!file_end.empty())
    {
        out << file_end;
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
        std::cout << std::format("start state: {}\n", dfa.get_start_state());
        std::cout << std::format("bitmask {}\n", format_bitmask(dfa.get_end_bitmask()));
        std::cout << std::format("states {}\n", dfa.get_end_bitmask().size());
        std::cout << std::format("state mapping {}\n", lexergen::format_table(dfa.get_end_to_nfa_state()));
        std::cout << std::format("emitted states: {}, emitted case labels: {}\n", res.state_count, res.case_count);
    }
}
