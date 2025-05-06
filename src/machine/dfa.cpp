// cSpell:ignore pbytes pcol pline
#include "machine/dfa.h"
#include "fwd.h"
#include "machine/cg.h"
#include "machine/data.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <execution>
#include <format>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct equivalent_class_result
{
    std::vector<uint8_t> classifier;
    std::vector<int64_t> transition;
    std::size_t class_count;
};

struct char_transition_info
{
    uint8_t ch;
    std::span<const int64_t> transition;

    constexpr auto operator==(const char_transition_info& rhs) const -> bool
    {
        for (size_t i = 0; i < transition.size() / lexergen::BYTE_MAX; i++)
        {
            if (transition[(i * lexergen::BYTE_MAX) + ch] != rhs.transition[(i * lexergen::BYTE_MAX) + rhs.ch])
            {
                return false;
            }
        }

        return true;
    }
};

template <>
struct std::hash<char_transition_info>
{
    auto operator()(const char_transition_info& info) const -> size_t
    {
        size_t ret = 0;
        std::hash<size_t> hasher;

        for (size_t i = 0; i < info.transition.size() / lexergen::BYTE_MAX; i++)
        {
            ret ^= hasher(info.transition[(i * lexergen::BYTE_MAX) + info.ch]) + 0x9e3779b9 + (ret << 6) + (ret >> 2);
        }

        return ret;
    }
};

template <>
struct std::hash<lexergen::state_set>
{
    auto operator()(const lexergen::state_set& info) const -> size_t
    {
        size_t ret = 0;
        std::hash<size_t> hasher;

        for (auto state : info)
        {
            ret ^= hasher(state) + 0x9e3779b9 + (ret << 6) + (ret >> 2);
        }

        return ret;
    }
};

namespace
{
    auto build_equivalence_class(std::span<const int64_t> transition) -> equivalent_class_result
    {
        std::unordered_map<char_transition_info, uint8_t> classes;
        size_t states = transition.size() / lexergen::BYTE_MAX;
        std::vector<uint8_t> classifier(lexergen::BYTE_MAX);
        for (size_t i = 0; i < lexergen::BYTE_MAX; i++)
        {
            char_transition_info inst = {.ch = static_cast<uint8_t>(i), .transition = transition};
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
                equivalent_transition_table[(state * classes.size()) + class_id] =
                    transition[(state * lexergen::BYTE_MAX) + equiv_candidate.ch] == -1
                        ? -1
                        : transition[(state * lexergen::BYTE_MAX) + equiv_candidate.ch];
            }
        }

        return {
            .classifier = std::move(classifier),
            .transition = std::move(equivalent_transition_table),
            .class_count = classes.size(),
        };
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    auto partition_set(const lexergen::state_set& left, const lexergen::state_set& right) -> std::pair<lexergen::state_set, lexergen::state_set>
    {
        // compute intersection
        lexergen::state_set intersection;
        lexergen::state_set left_only;

        for (auto state : left)
        {
            if (right.contains(state))
            {
                intersection.insert(state);
            }
            else
            {
                left_only.insert(state);
            }
        }

        return std::make_pair(intersection, left_only);
    }
} // namespace

auto lexergen::dfa::source_states(size_t ch, const state_set& target) -> lexergen::state_set
{
    state_set split;
    for (int64_t state = 0; state < get_state_count(); state++)
    {
        if (target.contains(transition_table[(state * BYTE_MAX) + ch]))
        {
            split.insert(state);
        }
    }

    return split;
}

auto lexergen::dfa::hopcroft(const std::unordered_set<state_set>& initial) -> std::vector<lexergen::state_set>
{
    // hopcroft sucks, i don't understand actual CS sorry
    auto queue = initial;
    auto partitions = initial;

    while (!queue.empty())
    {
        // take a from w
        auto curr = *queue.begin();
        queue.erase(queue.begin());

        for (size_t ch = 0; ch < BYTE_MAX; ch++)
        {
            // let X be the set of states for which a transition on c leads to a state in A
            state_set split = source_states(ch, curr);

            // copy here
            for (const auto& partition : std::vector(partitions.begin(), partitions.end()))
            {
                auto [intersection, y_only] = partition_set(partition, split);
                if (intersection.empty() || y_only.empty())
                {
                    continue;
                }

                partitions.erase(partition);
                partitions.insert(intersection);
                partitions.insert(y_only);

                if (queue.contains(partition))
                {
                    queue.erase(partition);
                    queue.insert(intersection);
                    queue.insert(y_only);
                }
                else
                {
                    queue.insert(intersection.size() <= y_only.size() ? intersection : y_only);
                }
            }
        }
    }

    return std::vector(partitions.begin(), partitions.end());
}

auto lexergen::dfa::reconstruct(const std::vector<state_set>& partitions) -> void
{
    std::vector<int64_t> old_to_new(get_state_count());
    std::vector<int64_t> new_to_old(partitions.size());

    int64_t curr_id = 0;

    for (const auto& partition : partitions)
    {
        const auto partition_id = curr_id++;
        for (auto state : partition)
        {
            new_to_old[partition_id] = state;
            old_to_new[state] = partition_id;
        }
    }

    // remap all of the DFA
    std::vector<int64_t> new_transition_table(curr_id * BYTE_MAX);

    for (int64_t new_state = 0; new_state < curr_id; new_state++)
    {
        for (size_t ch = 0; ch < BYTE_MAX; ch++)
        {
            auto old_state = transition_table[(new_to_old[new_state] * BYTE_MAX) + ch];
            new_transition_table[(new_state * BYTE_MAX) + ch] = old_state == -1 ? -1 : old_to_new[old_state];
        }
    }

    std::vector<bool> new_end_bitmask(curr_id);
    for (int64_t new_state = 0; new_state < curr_id; new_state++)
    {
        new_end_bitmask[new_state] = end_bitmask[new_to_old[new_state]];
    }

    std::vector<int64_t> new_end_to_nfa_state(curr_id);
    for (int64_t new_state = 0; new_state < curr_id; new_state++)
    {
        new_end_to_nfa_state[new_state] = end_to_nfa_state[new_to_old[new_state]];
    }

    transition_table = new_transition_table;
    start_state = old_to_new[start_state];
    end_bitmask = new_end_bitmask;
    end_to_nfa_state = new_end_to_nfa_state;
}

void lexergen::dfa::optimize(bool debug)
{
    state_set nonfinal_states;
    std::unordered_map<int64_t, state_set> nfa_to_end_state;

    for (int64_t state = 0; state < get_state_count(); state++)
    {
        if (!end_bitmask[state])
        {
            nonfinal_states.insert(state);
        }
        else
        {
            nfa_to_end_state[end_to_nfa_state[state]].insert(state);
        }
    }

    std::unordered_set<state_set> initial{nonfinal_states};

    for (const auto& [nfa, dfa] : nfa_to_end_state)
    {
        if (nfa != -1)
        {
            initial.insert(dfa);
        }
    }

    auto partitions = hopcroft(initial);

    if (debug)
    {
        std::cout << "State equivalence classes:\n";

        for (const auto& partition : partitions)
        {
            std::cout << format_table(partition) << "\n";
        }
    }

    reconstruct(partitions);
}

auto lexergen::dfa::codegen(std::ostream& out, std::string inc, std::string handle_error, std::string handle_internal_error, bool equivalence_class)
    const -> codegen_result
{
    std::string switch_str;
    for (const auto& handler : handler_map)
    {
        switch_str += std::format(R"(case {}: {})", handler.first, handler.second);
    }

    if (equivalence_class)
    {
        auto [classifier, new_tab, classes] = build_equivalence_class(get_transition_table());

        std::cout << std::format("found {} equivalence classes:\n", classes);

        for (size_t i = 0; i < classes; i++)
        {
            std::cout << std::format("class {}: {}\n", i, lexergen::format_string_class([classifier, i](uint8_t ch) { return classifier[ch] == i; }));
        }
        out << std::format(
            FMT_CODEGEN_EQUIVALENCE_CLASS, inc, format_table(new_tab), start_state, format_table(end_bitmask), format_table(end_to_nfa_state),
            format_table(classifier), classes, handle_error, switch_str, handle_internal_error
        );

        return {
            .classifier = std::move(classifier),
            .transition = std::move(new_tab),
            .class_count = classes,
        };
    }

    out << std::format(
        FMT_CODEGEN_REGULAR, inc, format_table(transition_table), start_state, format_table(end_bitmask), format_table(end_to_nfa_state),
        handle_error, switch_str, handle_internal_error
    );

    return {
        .classifier = {},
        .transition = transition_table,
        .class_count = 0,
    };
}
