#include <fmt/core.h>
#include <machine/dfa.h>
#include <machine/nfa.h>
#include <unordered_set>
#include <utils.h>
namespace lexergen
{
    namespace
    {
        constexpr void replace_all(std::string& str, const std::string& from, const std::string& to)
        {
            if (from.empty())
            {
                return;
            }

            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos)
            {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        }

        constexpr auto format_string_class_dot(auto check) -> std::string
        {
            auto str = format_string_class(check);
            replace_all(str, "\\", "\\\\");
            return str;
        }
    } // namespace

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
                ofs << fmt::format("{} -> {} [label=\"{}\"]\n", i, entry, format_string_class_dot([this, i, entry](size_t ch) {
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
                ofs << fmt::format("{} -> {} [label=\"{}\"]\n", from, to, format_string_class_dot([&](size_t ch) { return ch_map[ch]; }));
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
