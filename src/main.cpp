#include "argparse.h"
#include "build_config.h"
#include "machine/cg.h"
#include "machine/dfa.h"
#include "machine/nfa.h"
#include "regex.h"
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
        .description = "target language: cpp, c, java, javascript (default: inferred from -o's extension, else cpp)",
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
    // RULE [priority] /expr/ [handler]
    // UNKNOWN [handler]
    // ERROR [handler]
    // MACRO name /expr/
    // STATE name {
    //     RULE [priority] /expr/ [handler]
    //     ...
    // }
    // %%
    // [epilogue]
    //

    std::string preamble = get_str_section(in_file);
    std::string handle_error;
    std::string handle_internal_error;
    bool has_unknown = false;
    bool has_error = false;
    using rule_table = std::vector<lexergen::rule_def>;
    std::vector<std::pair<std::string, rule_table>> state_tables{{"", {}}};
    std::string current_state;
    std::size_t current_index = 0;
    lexergen::macro_table macros = lexergen::builtin_macros();

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

        if (trimmed == "}")
        {
            if (current_state.empty())
            {
                std::cerr << "unexpected `}` outside of a STATE block\n";
                exit(-1);
            }
            current_state.clear();
            current_index = 0;
            continue;
        }

        auto [word, directive_rest] = split_first_word(trimmed);

        if (word == "STATE")
        {
            if (!current_state.empty())
            {
                std::cerr << "STATE blocks cannot be nested\n";
                exit(-1);
            }

            auto [name, brace] = split_first_word(directive_rest);
            if (name.empty() || trim(brace) != "{")
            {
                std::cerr << std::format("malformed line `{}`: expected `STATE name {{`\n", line);
                exit(-1);
            }

            for (const auto& [existing_name, existing_table] : state_tables)
            {
                (void)existing_table;
                if (existing_name == name)
                {
                    std::cerr << std::format("duplicate STATE `{}`\n", name);
                    exit(-1);
                }
            }

            current_state = std::string(name);
            state_tables.emplace_back(current_state, rule_table{});
            current_index = state_tables.size() - 1;
            continue;
        }

        auto& tokens = state_tables[current_index].second;

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

        int64_t priority = 0;
        std::string_view rule_line = trimmed;

        if (word == "RULE")
        {
            rule_line = directive_rest;
            auto [maybe_priority, after_priority] = split_first_word(rule_line);
            auto digits = maybe_priority.starts_with('-') ? maybe_priority.substr(1) : maybe_priority;
            if (!digits.empty() && digits.find_first_not_of("0123456789") == std::string_view::npos)
            {
                priority = std::stoll(std::string(maybe_priority));
                rule_line = after_priority;
            }
        }

        auto [success, expr, error_msg, handler] = lexergen::parse_regex(std::string(rule_line), macros);

        if (!success)
        {
            std::cerr << std::format("failed to parse line `{}`: {}\n", line, error_msg);
            exit(-1);
        }

        tokens.push_back({.expr = expr, .handler = handler, .priority = priority});
    }

    if (!current_state.empty())
    {
        std::cerr << std::format("unterminated STATE block `{}`: missing closing `}}`\n", current_state);
        exit(-1);
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
            std::cerr << std::format("unknown --lang '{}' (expected cpp, c, java, javascript)\n", args["lang"].value);
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

    const bool multi_state = state_tables.size() > 1;
    auto base_fn_name = std::string(lexergen::base_fn_name(lang));

    for (std::size_t i = 0; i < state_tables.size(); i++)
    {
        const auto& [state_name, tokens] = state_tables[i];
        auto fn_name = state_name.empty() ? base_fn_name : base_fn_name + "_" + state_name;

        auto [dfa, nfa] = lexergen::make_lexer(tokens);

        if (args["optimize"].present)
        {
            dfa.optimize(args["debug"].present);
        }

        auto res = dfa.codegen(out, preamble, handle_error, handle_internal_error, lang, fn_name, i == 0);

        if (args["dfa-out"].present)
        {
            auto path =
                multi_state ? std::format("{}.{}.dot", args["dfa-out"].value, state_name.empty() ? "default" : state_name) : args["dfa-out"].value;
            std::ofstream dot_out(path);
            if (!dot_out)
            {
                std::cerr << "unable to open file: " << path << '\n';
                exit(-1);
            }

            dfa.dump(dot_out);
        }

        if (args["nfa-out"].present)
        {
            auto path =
                multi_state ? std::format("{}.{}.dot", args["nfa-out"].value, state_name.empty() ? "default" : state_name) : args["nfa-out"].value;
            std::ofstream dot_out(path);
            if (!dot_out)
            {
                std::cerr << "unable to open file: " << path << '\n';
                exit(-1);
            }

            nfa.dump(dot_out);
        }

        if (args["debug"].present)
        {
            std::cout << std::format("[{}] start state: {}\n", fn_name, dfa.get_start_state());
            std::cout << std::format("[{}] states {}\n", fn_name, dfa.get_end_bitmask().size());
            std::cout << std::format("[{}] emitted states: {}, emitted case labels: {}\n", fn_name, res.state_count, res.case_count);
        }
    }

    if (!file_end.empty())
    {
        out << file_end;
    }
}
