#include "fwd.h"
#include "machine/dfa.h"
#include "machine/nfa.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

void lexergen::dfa::dump(std::ostream& ofs)
{
    ofs << "digraph G{";
    const auto row_width = static_cast<int64_t>(get_class_count()) + 1;

    for (int64_t i = 0; i < get_state_count(); i++)
    {
        std::unordered_map<int64_t, std::vector<int64_t>> target_classes;

        for (int64_t class_id = 0; class_id < row_width; class_id++)
        {
            auto target = transition_table[static_cast<std::size_t>((i * row_width) + class_id)];
            if (target != -1)
            {
                target_classes[target].push_back(class_id);
            }
        }

        for (const auto& [entry, class_ids] : target_classes)
        {
            ofs << std::format("{} -> {} [label=\"{}\"]\n", i, entry, lexergen::format_class_list(class_ids, classes));
        }

        if (end_bitmask[i])
        {
            ofs << std::format("{} [shape=box]\n", i);
        }
    }

    ofs << '}';
}

void lexergen::nfa_builder::dump(std::ostream& ofs)
{
    ofs << "digraph G{";

    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::vector<int64_t>>> transition_table;

    for (const auto& edge : edges)
    {
        transition_table[static_cast<uint64_t>(edge.from)][static_cast<uint64_t>(edge.to)].push_back(edge.class_id);
    }

    for (const auto& [from, to_map] : transition_table)
    {
        for (const auto& [to, class_ids] : to_map)
        {
            ofs << std::format("{} -> {} [label=\"{}\"]\n", from, to, lexergen::format_class_list(class_ids, classes));
        }
    }

    for (auto node : start)
    {
        ofs << std::format("{} [shape=triangle]\n", node);
    }

    for (const auto& e : end)
    {
        ofs << std::format("{} [shape=box,label=\"{} (priority {})\"]\n", e.node, e.node, e.priority);
    }

    for (auto [from, to] : epsilon_edges)
    {
        ofs << std::format("{} -> {} [label=\"eps\"]\n", from, to);
    }

    ofs << '}';
}
