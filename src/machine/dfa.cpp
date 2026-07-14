// cSpell:ignore pbytes pcol pline
#include "machine/dfa.h"
#include "fwd.h"
#include "machine/cg.h"
#include "machine/data.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

template <>
struct std::hash<lexergen::state_set>
{
    auto operator()(const lexergen::state_set& info) const -> size_t
    {
        size_t ret = 0;
        std::hash<size_t> hasher;

        for (auto state : info)
        {
            ret ^= hasher(state) + 0x9e3779b9 + (ret << 6) + (ret >> 2);
        }

        return ret;
    }
};

namespace
{
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    auto partition_set(const lexergen::state_set& left, const lexergen::state_set& right) -> std::pair<lexergen::state_set, lexergen::state_set>
    {
        // compute intersection
        lexergen::state_set intersection;
        lexergen::state_set left_only;

        for (auto state : left)
        {
            if (right.contains(state))
            {
                intersection.insert(state);
            }
            else
            {
                left_only.insert(state);
            }
        }

        return std::make_pair(intersection, left_only);
    }
} // namespace

auto lexergen::dfa::source_states(size_t ch, const state_set& target) -> lexergen::state_set
{
    state_set split;
    for (int64_t state = 0; state < get_state_count(); state++)
    {
        if (target.contains(transition_table[(state * BYTE_MAX) + ch]))
        {
            split.insert(state);
        }
    }

    return split;
}

auto lexergen::dfa::hopcroft(const std::unordered_set<state_set>& initial) -> std::vector<lexergen::state_set>
{
    // hopcroft sucks, i don't understand actual CS sorry
    auto queue = initial;
    auto partitions = initial;

    while (!queue.empty())
    {
        // take a from w
        auto curr = *queue.begin();
        queue.erase(queue.begin());

        for (size_t ch = 0; ch < BYTE_MAX; ch++)
        {
            // let X be the set of states for which a transition on c leads to a state in A
            state_set split = source_states(ch, curr);

            // copy here
            for (const auto& partition : std::vector(partitions.begin(), partitions.end()))
            {
                auto [intersection, y_only] = partition_set(partition, split);
                if (intersection.empty() || y_only.empty())
                {
                    continue;
                }

                partitions.erase(partition);
                partitions.insert(intersection);
                partitions.insert(y_only);

                if (queue.contains(partition))
                {
                    queue.erase(partition);
                    queue.insert(intersection);
                    queue.insert(y_only);
                }
                else
                {
                    queue.insert(intersection.size() <= y_only.size() ? intersection : y_only);
                }
            }
        }
    }

    return std::vector(partitions.begin(), partitions.end());
}

auto lexergen::dfa::reconstruct(const std::vector<state_set>& partitions) -> void
{
    std::vector<int64_t> old_to_new(get_state_count());
    std::vector<int64_t> new_to_old(partitions.size());

    int64_t curr_id = 0;

    for (const auto& partition : partitions)
    {
        const auto partition_id = curr_id++;
        for (auto state : partition)
        {
            new_to_old[partition_id] = state;
            old_to_new[state] = partition_id;
        }
    }

    // remap all of the DFA
    std::vector<int64_t> new_transition_table(curr_id * BYTE_MAX);

    for (int64_t new_state = 0; new_state < curr_id; new_state++)
    {
        for (size_t ch = 0; ch < BYTE_MAX; ch++)
        {
            auto old_state = transition_table[(new_to_old[new_state] * BYTE_MAX) + ch];
            new_transition_table[(new_state * BYTE_MAX) + ch] = old_state == -1 ? -1 : old_to_new[old_state];
        }
    }

    std::vector<bool> new_end_bitmask(curr_id);
    for (int64_t new_state = 0; new_state < curr_id; new_state++)
    {
        new_end_bitmask[new_state] = end_bitmask[new_to_old[new_state]];
    }

    std::vector<int64_t> new_end_to_nfa_state(curr_id);
    for (int64_t new_state = 0; new_state < curr_id; new_state++)
    {
        new_end_to_nfa_state[new_state] = end_to_nfa_state[new_to_old[new_state]];
    }

    transition_table = new_transition_table;
    start_state = old_to_new[start_state];
    end_bitmask = new_end_bitmask;
    end_to_nfa_state = new_end_to_nfa_state;
}

void lexergen::dfa::optimize(bool debug)
{
    state_set nonfinal_states;
    std::unordered_map<int64_t, state_set> nfa_to_end_state;

    for (int64_t state = 0; state < get_state_count(); state++)
    {
        if (!end_bitmask[state])
        {
            nonfinal_states.insert(state);
        }
        else
        {
            nfa_to_end_state[end_to_nfa_state[state]].insert(state);
        }
    }

    std::unordered_set<state_set> initial{nonfinal_states};

    for (const auto& [nfa, dfa] : nfa_to_end_state)
    {
        if (nfa != -1)
        {
            initial.insert(dfa);
        }
    }

    auto partitions = hopcroft(initial);

    if (debug)
    {
        std::cout << "State equivalence classes:\n";

        for (const auto& partition : partitions)
        {
            std::cout << format_table(partition) << "\n";
        }
    }

    reconstruct(partitions);
}

namespace
{
    struct dfa_view
    {
        int64_t start_state;
        std::size_t state_count;
        const std::vector<int64_t>& transition_table;
        const std::vector<bool>& end_bitmask;
        const std::vector<int64_t>& end_to_nfa_state;
        const std::unordered_map<int64_t, std::string>& handler_map;
    };

    using byte_groups = std::unordered_map<int64_t, std::vector<uint8_t>>;

    auto build_byte_groups(const dfa_view& dfa, int64_t state) -> byte_groups
    {
        byte_groups groups;
        for (std::size_t ch = 0; ch < lexergen::BYTE_MAX; ch++)
        {
            auto target = dfa.transition_table[(static_cast<std::size_t>(state) * lexergen::BYTE_MAX) + ch];
            if (target != -1)
            {
                groups[target].push_back(static_cast<uint8_t>(ch));
            }
        }
        return groups;
    }

    auto indent_lines(std::string_view text, std::string_view prefix) -> std::string
    {
        std::string out;
        std::size_t pos = 0;
        bool any = false;

        while (pos <= text.size())
        {
            auto nl = text.find('\n', pos);
            auto line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);

            auto trimmed_end = line.find_last_not_of(" \t\r");
            if (trimmed_end != std::string_view::npos)
            {
                out += prefix;
                out += line.substr(0, trimmed_end + 1);
                any = true;
            }
            out += '\n';

            if (nl == std::string_view::npos)
            {
                break;
            }
            pos = nl + 1;
        }

        if (!any)
        {
            out = std::string(prefix) + "pass\n";
        }

        return out;
    }

    auto emit_cpp(
        std::ostream& out, const dfa_view& dfa, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error
    ) -> lexergen::codegen_result
    {
        out << "#include <cstdint>\n#include <cstddef>\n#include <string_view>\n\n" << inc << "\n\n";
        out << "template <typename Source, typename Ctx>\n";
        out << "auto lex_tok(Source& src, Ctx& ctx)\n{\n";
        out << "    (void)ctx;\n";
        out << "    int64_t latest_match = -1;\n\n";
        out << "    src.start_token();\n";
        out << "    [[maybe_unused]] std::size_t start_line = src.line();\n";
        out << "    [[maybe_unused]] std::size_t start_col = src.col();\n";
        out << "    [[maybe_unused]] std::size_t start_bytes = src.bytes();\n\n";
        out << std::format("    goto STATE_{};\n\n", dfa.start_state);

        std::size_t total_cases = 0;

        for (int64_t state = 0; state < static_cast<int64_t>(dfa.state_count); state++)
        {
            out << std::format("STATE_{}:\n", state);

            if (dfa.end_bitmask[state])
            {
                out << std::format("    latest_match = {};\n    src.accept();\n", dfa.end_to_nfa_state[state]);
            }

            auto groups = build_byte_groups(dfa, state);
            if (groups.empty())
            {
                out << "    goto FAIL;\n\n";
                continue;
            }

            out << "    switch (src.peek())\n    {\n";
            for (const auto& [target, bytes] : groups)
            {
                for (auto ch : bytes)
                {
                    out << std::format("    case {}: ", static_cast<int>(ch));
                    total_cases++;
                }
                out << std::format("goto STATE_{};\n", target);
            }
            out << "    default: goto FAIL;\n    }\n\n";
        }

        out << "FAIL:\n";
        out << "    if (latest_match == -1)\n    {\n";
        out << "        " << handle_error << "\n";
        out << "    }\n\n";
        out << "    src.backtrack();\n";
        out << "    {\n";
        out << "        [[maybe_unused]] std::string_view buffer = src.text();\n";
        out << "        switch (latest_match)\n        {\n";

        for (const auto& [nfa_state, handler] : dfa.handler_map)
        {
            out << std::format("        case {}: {}\n", nfa_state, handler);
        }

        out << "        default:\n            " << handle_internal_error << "\n";
        out << "        }\n    }\n\n";

        out << "    latest_match = -1;\n";
        out << "    src.start_token();\n";
        out << "    start_line = src.line();\n";
        out << "    start_col = src.col();\n";
        out << "    start_bytes = src.bytes();\n";
        out << std::format("    goto STATE_{};\n", dfa.start_state);
        out << "}\n";

        return {.state_count = dfa.state_count, .case_count = total_cases};
    }

    auto emit_c(
        std::ostream& out, const dfa_view& dfa, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error
    ) -> lexergen::codegen_result
    {
        out << "#include <stdint.h>\n#include <stddef.h>\n\n";
        out << "typedef struct { const char *ptr; size_t len; } lex_text;\n\n";
        out << inc << "\n\n";
        out << "#ifndef LEX_RESULT_TYPE\n#define LEX_RESULT_TYPE int\n#endif\n\n";
        out << "LEX_RESULT_TYPE lex_tok(Source *src, Ctx *ctx)\n{\n";
        out << "    (void)ctx;\n";
        out << "    int64_t latest_match = -1;\n\n";
        out << "    Source_start_token(src);\n";
        out << "    size_t start_line = Source_line(src);\n";
        out << "    size_t start_col = Source_col(src);\n";
        out << "    size_t start_bytes = Source_bytes(src);\n";
        out << "    (void)start_line; (void)start_col; (void)start_bytes;\n\n";
        out << std::format("    goto STATE_{};\n\n", dfa.start_state);

        std::size_t total_cases = 0;

        for (int64_t state = 0; state < static_cast<int64_t>(dfa.state_count); state++)
        {
            out << std::format("STATE_{}:\n", state);

            if (dfa.end_bitmask[state])
            {
                out << std::format("    latest_match = {};\n    Source_accept(src);\n", dfa.end_to_nfa_state[state]);
            }

            auto groups = build_byte_groups(dfa, state);
            if (groups.empty())
            {
                out << "    goto FAIL;\n\n";
                continue;
            }

            out << "    switch (Source_peek(src))\n    {\n";
            for (const auto& [target, bytes] : groups)
            {
                for (auto ch : bytes)
                {
                    out << std::format("    case {}: ", static_cast<int>(ch));
                    total_cases++;
                }
                out << std::format("goto STATE_{};\n", target);
            }
            out << "    default: goto FAIL;\n    }\n\n";
        }

        out << "FAIL:\n";
        out << "    if (latest_match == -1)\n    {\n";
        out << "        " << handle_error << "\n";
        out << "    }\n\n";
        out << "    Source_backtrack(src);\n";
        out << "    {\n";
        out << "        lex_text buffer = Source_text(src);\n";
        out << "        (void)buffer;\n";
        out << "        switch (latest_match)\n        {\n";

        for (const auto& [nfa_state, handler] : dfa.handler_map)
        {
            out << std::format("        case {}: {}\n", nfa_state, handler);
        }

        out << "        default:\n            " << handle_internal_error << "\n";
        out << "        }\n    }\n\n";

        out << "    latest_match = -1;\n";
        out << "    Source_start_token(src);\n";
        out << "    start_line = Source_line(src);\n";
        out << "    start_col = Source_col(src);\n";
        out << "    start_bytes = Source_bytes(src);\n";
        out << std::format("    goto STATE_{};\n", dfa.start_state);
        out << "}\n";

        return {.state_count = dfa.state_count, .case_count = total_cases};
    }

    auto emit_switch_loop(
        std::ostream& out, const dfa_view& dfa, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error,
        bool is_java
    ) -> lexergen::codegen_result
    {
        out << inc << "\n\n";
        if (is_java)
        {
            out << "static Object lexTok(Source src, Ctx ctx)\n{\n";
            out << "    int latestMatch = -1;\n";
            out << "    int state = " << dfa.start_state << ";\n\n";
            out << "    src.startToken();\n";
            out << "    long startLine = src.line();\n";
            out << "    long startCol = src.col();\n";
            out << "    long startBytes = src.bytes();\n\n";
        }
        else
        {
            out << "function lexTok(src, ctx)\n{\n";
            out << "    let latestMatch = -1;\n";
            out << "    let state = " << dfa.start_state << ";\n\n";
            out << "    src.startToken();\n";
            out << "    let startLine = src.line();\n";
            out << "    let startCol = src.col();\n";
            out << "    let startBytes = src.bytes();\n\n";
        }

        out << "    while (true) {\n    switch (state) {\n";

        std::size_t total_cases = 0;

        for (int64_t s = 0; s < static_cast<int64_t>(dfa.state_count); s++)
        {
            out << std::format("    case {}: {{\n", s);

            if (dfa.end_bitmask[s])
            {
                out << std::format("        latestMatch = {};\n        src.accept();\n", dfa.end_to_nfa_state[s]);
            }

            auto groups = build_byte_groups(dfa, s);
            if (groups.empty())
            {
                out << "        state = -1; continue;\n    }\n";
                continue;
            }

            out << "        switch (src.peek()) {\n";
            for (const auto& [target, bytes] : groups)
            {
                for (auto ch : bytes)
                {
                    out << std::format("        case {}: ", static_cast<int>(ch));
                    total_cases++;
                }
                out << std::format("state = {}; continue;\n", target);
            }
            out << "        default: state = -1; continue;\n        }\n    }\n";
        }

        out << "    case -1: {\n";
        out << "        if (latestMatch == -1) {\n";
        out << "            " << handle_error << "\n";
        out << "        }\n\n";
        out << "        src.backtrack();\n";
        out << (is_java ? "        String buffer = src.text();\n" : "        const buffer = src.text();\n");
        out << "        switch (latestMatch) {\n";

        for (const auto& [nfa_state, handler] : dfa.handler_map)
        {
            out << std::format("        case {}: {}\n", nfa_state, handler);
        }

        out << "        default:\n            " << handle_internal_error << "\n";
        out << "        }\n\n";

        out << "        latestMatch = -1;\n";
        out << "        src.startToken();\n";
        out << "        startLine = src.line();\n";
        out << "        startCol = src.col();\n";
        out << "        startBytes = src.bytes();\n";
        out << std::format("        state = {};\n", dfa.start_state);
        out << "        continue;\n    }\n";
        out << "    }\n    }\n}\n";

        return {.state_count = dfa.state_count, .case_count = total_cases};
    }

    auto emit_python(
        std::ostream& out, const dfa_view& dfa, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error
    ) -> lexergen::codegen_result
    {
        out << inc << "\n\n";
        out << "_TRANSITIONS = [\n";

        std::size_t total_cases = 0;

        for (int64_t state = 0; state < static_cast<int64_t>(dfa.state_count); state++)
        {
            auto groups = build_byte_groups(dfa, state);
            out << "    {";
            for (const auto& [target, bytes] : groups)
            {
                for (auto ch : bytes)
                {
                    out << std::format("{}: {}, ", static_cast<int>(ch), target);
                    total_cases++;
                }
            }
            out << "},\n";
        }
        out << "]\n\n";

        out << "_ACCEPT = {";
        for (int64_t state = 0; state < static_cast<int64_t>(dfa.state_count); state++)
        {
            if (dfa.end_bitmask[state])
            {
                out << std::format("{}: {}, ", state, dfa.end_to_nfa_state[state]);
            }
        }
        out << "}\n\n";

        out << std::format("_START = {}\n\n", dfa.start_state);

        out << "def lex_tok(src, ctx):\n";
        out << "    latest_match = -1\n";
        out << "    state = _START\n\n";
        out << "    src.start_token()\n";
        out << "    start_line = src.line()\n";
        out << "    start_col = src.col()\n";
        out << "    start_bytes = src.bytes()\n\n";
        out << "    while True:\n";
        out << "        nfa_state = _ACCEPT.get(state)\n";
        out << "        if nfa_state is not None:\n";
        out << "            latest_match = nfa_state\n";
        out << "            src.accept()\n\n";
        out << "        state = _TRANSITIONS[state].get(src.peek(), -1)\n\n";
        out << "        if state == -1:\n";
        out << "            if latest_match == -1:\n";
        out << indent_lines(handle_error, "                ");
        out << "\n";
        out << "            src.backtrack()\n";
        out << "            buffer = src.text()\n";

        bool first = true;
        for (const auto& [nfa_state, handler] : dfa.handler_map)
        {
            out << std::format("            {} latest_match == {}:\n", first ? "if" : "elif", nfa_state);
            out << indent_lines(handler, "                ");
            first = false;
        }
        out << (first ? "            if True:\n" : "            else:\n");
        out << indent_lines(handle_internal_error, "                ");

        out << "\n";
        out << "            latest_match = -1\n";
        out << "            src.start_token()\n";
        out << "            start_line = src.line()\n";
        out << "            start_col = src.col()\n";
        out << "            start_bytes = src.bytes()\n";
        out << "            state = _START\n";

        return {.state_count = dfa.state_count, .case_count = total_cases};
    }
} // namespace

auto lexergen::dfa::codegen(
    std::ostream& out, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error, target_lang lang
) const -> codegen_result
{
    const dfa_view view{
        .start_state = start_state,
        .state_count = get_state_count(),
        .transition_table = transition_table,
        .end_bitmask = end_bitmask,
        .end_to_nfa_state = end_to_nfa_state,
        .handler_map = handler_map,
    };

    switch (lang)
    {
    case target_lang::CPP:
        return emit_cpp(out, view, inc, handle_error, handle_internal_error);
    case target_lang::C:
        return emit_c(out, view, inc, handle_error, handle_internal_error);
    case target_lang::JAVA:
        return emit_switch_loop(out, view, inc, handle_error, handle_internal_error, true);
    case target_lang::JS:
        return emit_switch_loop(out, view, inc, handle_error, handle_internal_error, false);
    case target_lang::PYTHON:
        return emit_python(out, view, inc, handle_error, handle_internal_error);
    }

    return {.state_count = 0, .case_count = 0};
}
