// cSpell:ignore pbytes pcol pline
#include "utils.h"
#include <fmt/ranges.h>
#include <iostream>
#include <machine/dfa.h>

inline constexpr auto FMT_CODEGEN_REGULAR = R"(
#include <deque>
#include <istream>
#include <vector>
#include <cstdint>
#include <string>
#include <iostream>

{}

// transition state tables
static constexpr int64_t TRANSITION_TABLE[] = {{ {} }};
static constexpr int64_t START_STATE = {};
static constexpr bool END_BITMASK[] = {{ {} }};
static constexpr int64_t END_TO_NFA_STATE[] = {{ {} }};

static char peek(lex_context& ctx)
{{
    if (ctx.peek_index < ctx.buffer.size())
        return ctx.buffer[ctx.peek_index++];
    char ch = 0;
    ctx.in.get(ch);

    ctx.pbytes++;
    ctx.pcol++;
    if(ch == '\n')
    {{
        ctx.pline++;
        ctx.pcol = 1;
    }}

    ctx.buffer.push_back(ch);
    ctx.peek_index++;
    return ch;
}}

static void reset(lex_context& ctx)
{{
    ctx.peek_index = 0;
    /*ctx.pbytes = ctx.bytes;
    ctx.pcol = ctx.col;
    ctx.pline = ctx.line;*/
}}

static void seek_up(lex_context& ctx)
{{
    ctx.buffer.erase(ctx.buffer.begin(), ctx.buffer.begin() + ctx.peek_index);
    ctx.peek_index = 0;
    ctx.bytes = ctx.pbytes;
    ctx.col = ctx.pcol;
    ctx.line = ctx.pline;
}}

template <typename C>
static void commit_to_buffer(C& output, lex_context& ctx)
{{
    output.insert(output.end(), ctx.buffer.begin(), ctx.buffer.begin() + ctx.peek_index);
    seek_up(ctx);
}}

auto lex_tok(lex_context& ctx)
{{
    if(ctx.in.eof())
    {{ 
        {} 
    }}

    int64_t state = START_STATE;
    int64_t latest_match = -1;
    std::string buffer;

    size_t start_line = ctx.line;
    size_t start_col = ctx.col;
    size_t start_bytes = ctx.bytes;

    while (true)
    {{
        state = TRANSITION_TABLE[state * 256 + (uint8_t) peek(ctx)];
        if (state != -1 && END_BITMASK[state])
        {{
            latest_match = state;
            commit_to_buffer(buffer, ctx);
        }}

        if (state == -1 || ctx.in.eof())
        {{
            if (latest_match == -1)
            {{
                // report error
                {}
            }}

            reset(ctx);
            switch(END_TO_NFA_STATE[latest_match])
            {{
            {}
            default: {{
                {}
            }}
            }}

            latest_match = -1;
            state = START_STATE;
            buffer.clear();

            start_line = ctx.line;
            start_col = ctx.col;
            start_bytes = ctx.bytes;
            
            if(ctx.in.eof())
            {{ 
                {} 
            }}

            continue;
        }}
    }}
}})";

inline constexpr auto FMT_CODEGEN_EQUIVALENCE_CLASS = R"(
#include <deque>
#include <istream>
#include <vector>
#include <cstdint>
#include <string>
#include <iostream>

{}

// transition state tables
static constexpr int64_t TRANSITION_TABLE[] = {{ {} }};
static constexpr int64_t START_STATE = {};
static constexpr bool END_BITMASK[] = {{ {} }};
static constexpr int64_t END_TO_NFA_STATE[] = {{ {} }};
static constexpr uint8_t CLASSIFIER[256] = {{ {} }};
static constexpr size_t CLASS_COUNT = {};

static char peek(lex_context& ctx)
{{
    if (ctx.peek_index < ctx.buffer.size())
        return ctx.buffer[ctx.peek_index++];
    char ch = 0;
    ctx.in.get(ch);

    ctx.pbytes++;
    ctx.pcol++;
    if(ch == '\n')
    {{
        ctx.pline++;
        ctx.pcol = 1;
    }}

    ctx.buffer.push_back(ch);
    ctx.peek_index++;
    return ch;
}}

static void reset(lex_context& ctx)
{{
    ctx.peek_index = 0;
    /*ctx.pbytes = ctx.bytes;
    ctx.pcol = ctx.col;
    ctx.pline = ctx.line;*/
}}

static void seek_up(lex_context& ctx)
{{
    ctx.buffer.erase(ctx.buffer.begin(), ctx.buffer.begin() + ctx.peek_index);
    ctx.peek_index = 0;
    ctx.bytes = ctx.pbytes;
    ctx.col = ctx.pcol;
    ctx.line = ctx.pline;
}}

template <typename C>
static void commit_to_buffer(C& output, lex_context& ctx)
{{
    output.insert(output.end(), ctx.buffer.begin(), ctx.buffer.begin() + ctx.peek_index);
    seek_up(ctx);
}}

auto lex_tok(lex_context& ctx)
{{
    if(ctx.in.eof())
    {{ 
        {} 
    }}

    int64_t state = START_STATE;
    int64_t latest_match = -1;
    std::string buffer;

    size_t start_line = ctx.line;
    size_t start_col = ctx.col;
    size_t start_bytes = ctx.bytes;

    while (true)
    {{
        state = TRANSITION_TABLE[state * CLASS_COUNT + CLASSIFIER[(uint8_t) peek(ctx)]];
        if (state != -1 && END_BITMASK[state])
        {{
            latest_match = state;
            commit_to_buffer(buffer, ctx);
        }}

        if (state == -1 || ctx.in.eof())
        {{
            if (latest_match == -1)
            {{
                // report error
                {}
            }}

            reset(ctx);
            switch(END_TO_NFA_STATE[latest_match])
            {{
            {}
            default:
                {}
            }}

            latest_match = -1;
            state = START_STATE;
            buffer.clear();

            start_line = ctx.line;
            start_col = ctx.col;
            start_bytes = ctx.bytes;
            
            if(ctx.in.eof())
            {{ 
                {} 
            }}

            continue;
        }}
    }}
}})";

namespace lexergen
{
    void dfa::simulate(std::string_view buffer, auto callback) const
    {
        int64_t state = start_state;
        int64_t latest_match = -1;
        std::string match_buf;

        for (int64_t index = 0; index < buffer.size(); index++)
        {
            state = transition_table[state * BYTE_MAX + (uint8_t)buffer[index]];

            if (state == -1)
            {
                if (latest_match == -1)
                {
                    break;
                }
                callback(latest_match, match_buf);

                // reset the state machine so we can process more information
                latest_match = -1;
                index--;
                state = start_state;
                match_buf.clear();
                continue;
            }

            if (end_bitmask[state])
            {
                latest_match = state;
            }
            match_buf += buffer[index];
        }
    }

    void dfa::codegen(
        std::ostream& out, std::string inc, std::string handle_eof, std::string handle_error, std::string handle_internal_error,
        bool equivalence_class
    ) const
    {
        std::string switch_str;
        for (const auto& handler : handler_map)
        {
            switch_str += fmt::format(R"(case {}: {})", handler.first, handler.second);
        }

        if (equivalence_class)
        {
            auto [classifier, new_tab, classes] = build_equivalence_class(get_transition_table());

            std::cout << fmt::format("found {} equivalence classes:\n", classes);

            for (size_t i = 0; i < classes; i++)
            {
                std::cout << fmt::format("class {}: {}\n", i, lexergen::format_string_class([classifier, i](uint8_t ch) {
                                             return classifier[ch] == i;
                                         }));
            }
            out << fmt::format(
                FMT_CODEGEN_EQUIVALENCE_CLASS, inc, fmt::join(new_tab, ","), start_state, fmt::join(end_bitmask, ","),
                fmt::join(end_to_nfa_state, ","), fmt::join(classifier, ","), classes, handle_eof, handle_error, switch_str, handle_internal_error,
                handle_eof
            );
        }
        else
        {
            out << fmt::format(
                FMT_CODEGEN_REGULAR, inc, fmt::join(transition_table, ","), start_state, fmt::join(end_bitmask, ","),
                fmt::join(end_to_nfa_state, ","), handle_eof, handle_error, switch_str, handle_internal_error, handle_eof
            );
        }
    }
} // namespace lexergen
