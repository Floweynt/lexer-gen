#include "fwd.h"
#include "machine/dfa.h"
#include "machine/nfa.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace
{
    constexpr const char* DOT_PRELUDE = "graph [fontname=\"Courier\"];\nnode [fontname=\"Courier\"];\nedge [fontname=\"Courier\"];\n";

    void write_cluster_header(std::ostream& ofs, int64_t cluster_id, std::string_view label)
    {
        ofs << std::format("subgraph cluster_{} {{\n", cluster_id);
        if (!label.empty())
        {
            ofs << std::format("label=\"{}\";\n", label);
        }
    }

    void write_start_marker(std::ostream& ofs, int64_t cluster_id, int64_t start_node)
    {
        ofs << std::format("__start_{} [shape=point,label=\"\"];\n", cluster_id);
        ofs << std::format("__start_{} -> {};\n", cluster_id, start_node);
    }
} // namespace

void lexergen::dfa::dump_cluster(std::ostream& ofs, int64_t node_offset, std::string_view label) const
{
    write_cluster_header(ofs, node_offset, label);
    write_start_marker(ofs, node_offset, node_offset + start_state);

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
            ofs << std::format("{} -> {} [label=<{}>]\n", node_offset + i, node_offset + entry, lexergen::format_class_list(class_ids, classes));
        }

        ofs << std::format("{} [label=\"{}\"{}]\n", node_offset + i, i, end_bitmask[i] ? ",shape=box" : "");
    }

    ofs << "}\n";
}

void lexergen::dfa::dump(std::ostream& ofs) const
{
    ofs << "digraph G{\n" << DOT_PRELUDE;
    dump_cluster(ofs, 0, "");
    ofs << '}';
}

void lexergen::dump_all(std::ostream& ofs, const std::vector<std::pair<std::string, const dfa*>>& entries)
{
    ofs << "digraph G{\n" << DOT_PRELUDE;

    constexpr int64_t STRIDE = 1'000'000;
    int64_t offset = 0;
    int64_t cluster_id = 0;

    for (const auto& [name, d] : entries)
    {
        d->dump_cluster(ofs, offset, name);
        offset += STRIDE;
        cluster_id++;
    }

    ofs << '}';
}

void lexergen::nfa_builder::dump_cluster(std::ostream& ofs, int64_t node_offset, std::string_view label) const
{
    write_cluster_header(ofs, node_offset, label);

    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::vector<int64_t>>> transition_table;

    for (const auto& edge : edges)
    {
        transition_table[static_cast<uint64_t>(edge.from)][static_cast<uint64_t>(edge.to)].push_back(edge.class_id);
    }

    for (const auto& [from, to_map] : transition_table)
    {
        for (const auto& [to, class_ids] : to_map)
        {
            ofs << std::format(
                "{} -> {} [label=<{}>]\n", node_offset + static_cast<int64_t>(from), node_offset + static_cast<int64_t>(to),
                lexergen::format_class_list(class_ids, classes)
            );
        }
    }

    for (auto node : start)
    {
        write_start_marker(ofs, node_offset, node_offset + node);
    }

    for (auto [from, to] : epsilon_edges)
    {
        ofs << std::format("{} -> {} [label=\"eps\"]\n", node_offset + from, node_offset + to);
    }

    std::unordered_map<int64_t, int64_t> priority_by_end_node;
    for (const auto& e : end)
    {
        priority_by_end_node[e.node] = e.priority;
    }

    for (int64_t i = 0; i <= max_val; i++)
    {
        if (auto it = priority_by_end_node.find(i); it != priority_by_end_node.end())
        {
            ofs << std::format("{} [shape=box,label=\"{} (priority {})\"]\n", node_offset + i, i, it->second);
        }
        else
        {
            ofs << std::format("{} [label=\"{}\"]\n", node_offset + i, i);
        }
    }

    ofs << "}\n";
}

void lexergen::nfa_builder::dump(std::ostream& ofs) const
{
    ofs << "digraph G{\n" << DOT_PRELUDE;
    dump_cluster(ofs, 0, "");
    ofs << '}';
}

void lexergen::dump_all(std::ostream& ofs, const std::vector<std::pair<std::string, const nfa_builder*>>& entries)
{
    ofs << "digraph G{\n" << DOT_PRELUDE;

    constexpr int64_t STRIDE = 1'000'000;
    int64_t offset = 0;

    for (const auto& [name, n] : entries)
    {
        n->dump_cluster(ofs, offset, name);
        offset += STRIDE;
    }

    ofs << '}';
}
