#pragma once

#include "dfa.h"
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <ostream>
#include <utility>
#include <vector>

namespace lexergen
{
    class nfa_builder
    {
        struct entry
        {
            int64_t from, to;
            char ch;
        };

        std::vector<entry> edges;
        std::vector<std::pair<int64_t, int64_t>> epsilon_edges;
        std::vector<int64_t> start;
        std::vector<int64_t> end;
        int64_t max_val = 0;

    public:
        constexpr nfa_builder() = default;

        template <std::convertible_to<char>... Args>
            requires(sizeof...(Args) > 1)
        constexpr auto transition(int64_t source, int64_t target, Args... val) -> nfa_builder&
        {
            return transition(source, target, {((char)val)...});
        }

        constexpr auto transition(int64_t source, int64_t target, const std::vector<char>& transition_list) -> nfa_builder&
        {
            for (auto transition_ch : transition_list)
            {
                transition(source, target, transition_ch);
            }
            return *this;
        }

        constexpr auto transition(int64_t from, int64_t end, char ch) -> nfa_builder&
        {
            max_val = std::max({max_val, from, end});
            edges.push_back({from, end, ch});
            return *this;
        }

        constexpr auto epsilon(int64_t from, int64_t end) -> nfa_builder&
        {
            max_val = std::max({max_val, from, end});
            epsilon_edges.emplace_back(from, end);
            return *this;
        }

        constexpr auto add_start(int64_t name) -> nfa_builder&
        {
            max_val = std::max(max_val, name);
            start.push_back(name);
            return *this;
        }

        constexpr auto add_end(int64_t name) -> nfa_builder&
        {
            max_val = std::max(max_val, name);
            end.push_back(name);
            return *this;
        }

        auto build() -> dfa;
        void dump(std::ostream& ofs);
    };
} // namespace lexergen
