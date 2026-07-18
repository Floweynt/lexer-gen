#include "machine/nfa.h"
#include "fwd.h"
#include "machine/data.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <list>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    auto build_epsilon_closure_from(const std::vector<std::unordered_set<int64_t>>& epsilon, int64_t node, std::vector<bool>& bitmask)
        -> std::vector<bool>&
    {
        std::queue<int64_t> to_process;
        to_process.push(node);
        while (!to_process.empty())
        {
            int64_t curr = to_process.front();
            to_process.pop();
            bitmask[curr] = true;
            for (auto ch : epsilon[curr])
            {
                if (!bitmask[ch])
                {
                    to_process.push(ch);
                }
            }
        }
        return bitmask;
    }
} // namespace

auto lexergen::nfa_builder::build() -> dfa
{
    const int64_t nodes = max_val + 1;
    const auto class_count = static_cast<int64_t>(classes.class_count());

    std::list<entry> output_edges;
    std::unordered_map<std::vector<bool>, int64_t> subset_to_id;
    int64_t curr_node_id = 0;
    std::vector<state_set> transition_table(static_cast<std::size_t>(nodes * class_count));
    std::vector<state_set> epsilon_table(nodes);
    std::vector<bool> start_bitset(nodes);
    std::vector<bool> tmp_state_set(nodes);
    std::queue<std::vector<bool>> nodes_to_process;

    for (const auto& edge : epsilon_edges)
    {
        epsilon_table[edge.first].insert(edge.second);
    }
    for (auto node : start)
    {
        build_epsilon_closure_from(epsilon_table, node, start_bitset);
    }
    for (const auto& edge : edges)
    {
        transition_table[static_cast<std::size_t>((edge.from * class_count) + edge.class_id)].insert(edge.to);
    }

    nodes_to_process.push(start_bitset);
    subset_to_id[start_bitset] = curr_node_id++;

    while (!nodes_to_process.empty())
    {
        auto node = nodes_to_process.front();
        nodes_to_process.pop();
        for (int64_t class_id = 0; class_id < class_count; class_id++)
        {
            bool is_not_empty = false;
            for (int64_t i = 0; i < nodes; i++)
            {
                if (!node[i])
                {
                    continue;
                }
                for (auto target : transition_table[static_cast<std::size_t>((i * class_count) + class_id)])
                {
                    is_not_empty = true;
                    build_epsilon_closure_from(epsilon_table, target, tmp_state_set);
                }
            }

            if (is_not_empty)
            {
                if (!subset_to_id.contains(tmp_state_set))
                {
                    nodes_to_process.push(tmp_state_set);
                    subset_to_id[tmp_state_set] = curr_node_id++;
                }

                int64_t node_from = subset_to_id[node];
                int64_t node_to = subset_to_id[tmp_state_set];
                output_edges.push_back({.from = node_from, .to = node_to, .class_id = class_id});
                tmp_state_set.assign(nodes, false);
            }
        }
    }

    dfa ret(curr_node_id, classes);
    ret.start_state = 0;

    std::unordered_map<int64_t, std::vector<bool>> vec;

    for (const auto& entry : subset_to_id)
    {
        vec[entry.second] = entry.first;
    }

    std::unordered_map<int64_t, int64_t> priority_by_node;
    for (const auto& e : end)
    {
        priority_by_node[e.node] = e.priority;
    }

    for (const auto& entry : subset_to_id)
    {
        for (const auto& e : end)
        {
            auto nfa_node_id = e.node;
            if (entry.first[nfa_node_id])
            {
                if (ret.end_bitmask[entry.second])
                {
                    auto existing = ret.end_to_nfa_state[entry.second];

                    if (e.priority == priority_by_node[existing])
                    {
                        std::cerr << std::format("possible conflict between states {} and {} (priority {})\n", existing, nfa_node_id, e.priority);
                    }

                    if (e.priority > priority_by_node[existing] || (e.priority == priority_by_node[existing] && nfa_node_id < existing))
                    {
                        ret.end_to_nfa_state[entry.second] = nfa_node_id;
                    }

                    continue;
                }

                ret.end_bitmask[entry.second] = true;
                ret.end_to_nfa_state[entry.second] = nfa_node_id;
            }
        }
    }

    const int64_t dfa_row_width = class_count + 1; // +1: sentinel "no class" column, see dfa.h
    for (const auto& edge : output_edges)
    {
        ret.transition_table[static_cast<std::size_t>((edge.from * dfa_row_width) + edge.class_id)] = edge.to;
    }

    return ret;
}
