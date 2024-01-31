#include <fmt/core.h>
#include <machine/dfa.h>
#include <machine/nfa.h>
#include <unordered_set>

namespace
{
    auto fmt_char(char ch) -> std::string
    {
        if (!isprint(ch))
        {
            return fmt::format("\\\\x{:02x}", (unsigned char)ch);
        }
        if (ch == '"')
        {
            return "\\\"";
        }
        if (ch == '\\')
        {
            return "\\\\";
        }
        return std::string(1, ch);
    }

    constexpr auto format_string_class(auto check) -> std::string
    {
        std::string out;

        for (size_t ch = 0; ch < lexergen::BYTE_MAX; ch++)
        {
            size_t start_ch = ch;
            while (check(ch) && ch < lexergen::BYTE_MAX)
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

} // namespace

namespace lexergen
{
    void dfa::dump(std::ostream& ofs)
    {
        ofs << "digraph G{";
        for (size_t i = 0; i < end_bitmask.size(); i++)
        {
            std::unordered_set<size_t> target_set;

            for (size_t ch = 0; ch < BYTE_MAX; ch++)
            {
                if (transition_table[i * BYTE_MAX + ch] != -1UL)
                {
                    target_set.insert(transition_table[i * BYTE_MAX + ch]);
                }
            }

            for (auto entry : target_set)
            {
                ofs << fmt::format("{} -> {} [label=\"{}\"]\n", i, entry, format_string_class([this, i, entry](size_t ch) {
                                       return transition_table[i * BYTE_MAX + ch] == entry;
                                   }));
            }

            if (end_bitmask[i])
            {
                ofs << fmt::format("{} [shape=box]\n", i);
            }
        }

        ofs << '}';
    }

    void nfa_builder::dump(std::ostream& ofs)
    {
        ofs << "digraph G{";

        std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::bitset<BYTE_MAX>>> transition_table;

        for (auto edge : edges)
        {
            transition_table[edge.from][edge.to][(uint8_t)edge.ch] = true;
        }

        for (const auto& [from, to_map] : transition_table)
        {
            for (const auto& [to, ch_map] : to_map)
            {
                ofs << fmt::format("{} -> {} [label=\"{}\"]\n", from, to, format_string_class([&](size_t ch) { return ch_map[ch]; }));
            }
        }

        for (auto node : start)
        {
            ofs << fmt::format("{} [shape=triangle]\n", node);
        }

        for (auto node : end)
        {
            ofs << fmt::format("{} [shape=box]\n", node);
        }

        for (auto [from, to] : epsilon_edges)
        {
            ofs << fmt::format("{} -> {} [label=\"eps\"]\n", from, to);
        }

        ofs << '}';
    }
} // namespace lexergen
