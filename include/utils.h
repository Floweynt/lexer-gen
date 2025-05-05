#pragma once

#include "fwd.h"
#include <cctype>
#include <cstddef>
#include <format>
#include <print>
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
                out += std::format("[{}-{}]", fmt_char((char)start_ch), fmt_char((char)(end_ch - 1)));
            }
        }

        return out;
    }

    template <typename T>
    constexpr auto format_table(const std::vector<T>& vec) -> std::string
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
