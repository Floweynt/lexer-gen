#include "machine/nfa.h"
#include "fwd.h"
#include <algorithm>
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

    std::list<entry> output_edges;
    std::unordered_map<std::vector<bool>, int64_t> subset_to_id;
    int64_t curr_node_id = 0;
    std::vector<std::unordered_set<int64_t>> transition_table(nodes * BYTE_MAX);
    std::vector<std::unordered_set<int64_t>> epsilon_table(nodes);
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
        transition_table[(edge.from * BYTE_MAX) + (uint8_t)edge.ch].insert(edge.to);
    }

    nodes_to_process.push(start_bitset);
    subset_to_id[start_bitset] = curr_node_id++;

    while (!nodes_to_process.empty())
    {
        auto node = nodes_to_process.front();
        nodes_to_process.pop();
        for (int64_t ch = 0; ch < BYTE_MAX; ch++)
        {
            bool is_not_empty = false;
            for (int64_t i = 0; i < nodes; i++)
            {
                if (!node[i])
                {
                    continue;
                }
                for (auto target : transition_table[(i * BYTE_MAX) + ch])
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
                output_edges.push_back({node_from, node_to, (char)ch});
                tmp_state_set.assign(nodes, false);
            }
        }
    }

    dfa ret(curr_node_id);
    ret.start_state = 0;

    std::unordered_map<int64_t, std::vector<bool>> vec;

    for (const auto& entry : subset_to_id)
    {
        vec[entry.second] = entry.first;
    }

    for (const auto& entry : subset_to_id)
    {
        for (auto nfa_node_id : end)
        {
            // if this state contains an end node...
            if (entry.first[nfa_node_id])
            {
                if (ret.end_bitmask[entry.second])
                {
                    std::cerr << std::format("possible conflict between states {} and {}\n", ret.end_to_nfa_state[entry.second], nfa_node_id);
                    // pick the one with the greater node id, because of rules
                    nfa_node_id = std::min(ret.end_to_nfa_state[entry.second], nfa_node_id);
                }

                ret.end_bitmask[entry.second] = true;
                // we store the end state type from the NFA
                ret.end_to_nfa_state[entry.second] = nfa_node_id;
            }
        }
    }

    for (const auto& edge : output_edges)
    {
        ret.transition_table[(edge.from * BYTE_MAX) + (uint8_t)edge.ch] = edge.to;
    }

    return ret;
}
