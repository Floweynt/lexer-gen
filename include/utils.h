#pragma once

#include "machine/equivalence_classes.h"
#include "machine/interval_set.h"
#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace lexergen
{
    constexpr auto fmt_char(char ch) -> std::string
    {
        switch (ch)
        {
        case '\n':
            return "\\n";
        case '\r':
            return "\\r";
        case '\v':
            return "\\v";
        case '\t':
            return "\\t";
        case '\b':
            return "\\b";
        case '\a':
            return "\\a";
        case '\f':
            return "\\f";
        case '"':
            return "\\\"";
        case '\\':
            return "\\\\";
        case '-':
            return "\\-";
        default:
            break;
        }

        if (!isprint(ch))
        {
            return std::format("\\x{:02x}", (unsigned char)ch);
        }

        return std::string(1, ch);
    }

    inline auto fmt_codepoint(interval_set::codepoint cp) -> std::string
    {
        if (cp <= 0x7F)
        {
            return fmt_char(static_cast<char>(cp));
        }
        return std::format("\\u{{{:04X}}}", cp);
    }

    inline auto format_class_list(const std::vector<int64_t>& class_ids, const equivalence_classes& classes) -> std::string
    {
        std::string out;
        for (auto class_id : class_ids)
        {
            if (class_id >= static_cast<int64_t>(classes.class_count()))
            {
                out += "<none>";
                continue;
            }

            auto interval = classes.class_interval(class_id);
            if (interval.lo == interval.hi)
            {
                out += fmt_codepoint(interval.lo);
            }
            else
            {
                out += std::format("[{}-{}]", fmt_codepoint(interval.lo), fmt_codepoint(interval.hi));
            }
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
