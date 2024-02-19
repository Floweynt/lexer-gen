#pragma once

#include "fwd.h"
#include <cstdint>
#include <fmt/core.h>
#include <span>
#include <string>
#include <vector>

namespace lexergen
{
    struct equivalent_class_result
    {
        std::vector<uint8_t> classifier;
        std::vector<int64_t> transition;
        size_t class_count;
    };

    auto handle_escape_str(const std::string& str) -> std::string;
    auto build_equivalence_class(std::span<const int64_t> transition) -> equivalent_class_result;

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
            return fmt::format("\\x{:02x}", (unsigned char)ch);
        }
        
        return std::string(1, ch);
    }

    constexpr auto format_string_class(auto check) -> std::string
    {
        std::string out;

        for (size_t ch = 0; ch < BYTE_MAX; ch++)
        {
            size_t start_ch = ch;
            while (ch < BYTE_MAX && check(ch))
            {
                ch++;
            }
            size_t end_ch = ch;
            if (start_ch == end_ch)
            {
                continue;
            }

            if (start_ch + 1 == end_ch)
            {
                out += fmt_char((char)start_ch);
            }
            else
            {
                out += fmt::format("[{}-{}]", fmt_char((char)start_ch), fmt_char((char)(end_ch - 1)));
            }
        }

        return out;
    }
} // namespace lexergen
