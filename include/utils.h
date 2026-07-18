#pragma once

#include "machine/equivalence_classes.h"
#include "machine/interval_set.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace lexergen
{
    constexpr const char* DOT_FONT = "Courier";
    constexpr const char* COLOR_LITERAL = "darkgreen";
    constexpr const char* COLOR_ESCAPE = "blue";
    constexpr const char* COLOR_PUNCT = "goldenrod";

    struct fmt_result
    {
        std::string text;
        bool is_escape;
    };

    inline auto html_escape(char ch) -> std::string
    {
        switch (ch)
        {
        case '&':
            return "&amp;";
        case '<':
            return "&lt;";
        case '>':
            return "&gt;";
        default:
            return std::string(1, ch);
        }
    }

    constexpr auto fmt_char(char ch) -> fmt_result
    {
        switch (ch)
        {
        case '\n':
            return {"\\n", true};
        case '\r':
            return {"\\r", true};
        case '\v':
            return {"\\v", true};
        case '\t':
            return {"\\t", true};
        case '\b':
            return {"\\b", true};
        case '\a':
            return {"\\a", true};
        case '\f':
            return {"\\f", true};
        case '\\':
            return {"\\\\\\\\", true};
        case ' ':
            return {"\\x20", true};
        default:
            break;
        }

        if (!isprint(static_cast<unsigned char>(ch)))
        {
            return {std::format("\\x{:02x}", (unsigned char)ch), true};
        }

        return {html_escape(ch), false};
    }

    inline auto fmt_codepoint(interval_set::codepoint cp) -> fmt_result
    {
        if (cp <= 0x7F)
        {
            return fmt_char(static_cast<char>(cp));
        }
        if (cp <= 0xFF)
        {
            return {std::format("\\x{:02x}", cp), true};
        }
        return {std::format("\\u{{{:04X}}}", cp), true};
    }

    inline auto colorize(const fmt_result& r) -> std::string
    {
        return std::format("<FONT COLOR=\"{}\">{}</FONT>", r.is_escape ? COLOR_ESCAPE : COLOR_LITERAL, r.text);
    }

    inline auto punct(const char* text) -> std::string { return std::format("<FONT COLOR=\"{}\">{}</FONT>", COLOR_PUNCT, text); }

    inline auto format_class_list(const std::vector<int64_t>& class_ids, const equivalence_classes& classes) -> std::string
    {
        std::vector<interval_set::interval> intervals;
        bool has_sentinel = false;

        for (auto class_id : class_ids)
        {
            if (class_id >= static_cast<int64_t>(classes.class_count()))
            {
                has_sentinel = true;
                continue;
            }
            intervals.push_back(classes.class_interval(class_id));
        }

        std::ranges::sort(intervals, [](const auto& lhs, const auto& rhs) { return lhs.lo < rhs.lo; });

        std::vector<interval_set::interval> merged;
        for (const auto& iv : intervals)
        {
            if (!merged.empty() && iv.lo <= merged.back().hi + 1)
            {
                merged.back().hi = std::max(merged.back().hi, iv.hi);
            }
            else
            {
                merged.push_back(iv);
            }
        }

        std::string out;
        for (const auto& iv : merged)
        {
            if (iv.lo == iv.hi)
            {
                out += colorize(fmt_codepoint(iv.lo));
            }
            else
            {
                out += punct("[");
                out += colorize(fmt_codepoint(iv.lo));
                out += punct("-");
                out += colorize(fmt_codepoint(iv.hi));
                out += punct("]");
            }
        }

        if (has_sentinel)
        {
            out += "&lt;none&gt;";
        }

        return out;
    }

    constexpr auto format_table(const auto& vec) -> std::string
    {
        std::string buf;
        for (auto ent : vec)
        {
            buf += std::to_string(ent);
            buf.push_back(',');
        }
        buf.pop_back();
        return buf;
    }
} // namespace lexergen
