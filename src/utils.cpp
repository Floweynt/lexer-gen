#include "fwd.h"
#include <cstdint>
#include <functional>
#include <iostream>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <utils.h>
#include <vector>

namespace lexergen
{
    auto handle_escape_str(const std::string& str) -> std::string
    {
        std::string result;

        for (int i = 0; i < str.length(); i++)
        {
            if (str[i] == '\\')
            {
                if (i + 1 >= str.length())
                {
                    std::cerr << "invalid escape sequence at end of string\n";
                    exit(-1);
                }

                switch (str[i + 1])
                {
                case 'n':
                    result += '\n';
                    break;
                case 't':
                    result += '\t';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case '\'':
                    result += '\'';
                    break;
                case '\"':
                    result += '\"';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '0':
                    result.push_back('\0');
                    break;
                case 'x': {
                    if (i + 3 >= str.length())
                    {
                        std::cerr << "incomplete hexadecimal escape sequence";
                        exit(-1);
                    }
                    char hex[3] = {str[i + 2], str[i + 3], '\0'};
                    int value = std::stoi(hex, nullptr, 16);
                    result.push_back(static_cast<char>(value));
                    i += 2; // skip the next 3 characters (the \x and two hex digits)
                    break;
                }
                }
                i++;
            }
            else
            {
                result.push_back(str[i]);
            }
        }

        return result;
    }

    struct char_transition_info
    {
        uint8_t ch;
        std::span<const int64_t> transition;

        constexpr auto operator==(const char_transition_info& rhs) const -> bool
        {
            for (size_t i = 0; i < transition.size() / BYTE_MAX; i++)
            {
                if (transition[i * BYTE_MAX + ch] != rhs.transition[i * BYTE_MAX + rhs.ch])
                {
                    return false;
                }
            }

            return true;
        }
    };
} // namespace lexergen

template <>
struct std::hash<lexergen::char_transition_info>
{
    auto operator()(const lexergen::char_transition_info& info) const -> size_t
    {
        size_t ret = 0;
        std::hash<size_t> hasher;

        for (size_t i = 0; i < info.transition.size() / lexergen::BYTE_MAX; i++)
        {
            ret ^= hasher(info.transition[i * lexergen::BYTE_MAX + info.ch]) + 0x9e3779b9 + (ret << 6) + (ret >> 2);
        }

        return ret;
    }
};

namespace lexergen
{
    auto build_equivalence_class(std::span<const int64_t> transition) -> equivalent_class_result
    {
        std::unordered_map<char_transition_info, uint8_t> classes;
        size_t states = transition.size() / BYTE_MAX;
        std::vector<uint8_t> classifier(BYTE_MAX);
        for (size_t i = 0; i < BYTE_MAX; i++)
        {
            char_transition_info inst = {static_cast<uint8_t>(i), transition};
            if (!classes.contains(inst))
            {
                classes[inst] = classes.size();
            }

            classifier[i] = classes[inst];
        }

        std::vector<int64_t> equivalent_transition_table(classes.size() * states);
        for (const auto& [equiv_candidate, class_id] : classes)
        {
            for (size_t state = 0; state < states; state++)
            {
                equivalent_transition_table[state * classes.size() + class_id] =
                    transition[state * BYTE_MAX + equiv_candidate.ch] == -1 ? -1 : transition[state * BYTE_MAX + equiv_candidate.ch];
            }
        }

        return {
            std::move(classifier),
            std::move(equivalent_transition_table),
            classes.size(),
        };
    }
} // namespace lexergen
