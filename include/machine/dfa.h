#pragma once

#include "fwd.h"
#include "regex.h"
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lexergen
{
    auto make_lexer(const std::vector<std::pair<regex, std::string>>& table) -> std::pair<dfa, nfa_builder>;

    struct codegen_result
    {
        std::vector<uint8_t> classifier;
        std::vector<int64_t> transition;
        std::size_t class_count;
    };

    class dfa
    {
        friend class nfa_builder;
        friend auto make_lexer(const std::vector<std::pair<regex, std::string>>& table) -> std::pair<dfa, nfa_builder>;

        std::vector<int64_t> transition_table;
        int64_t start_state{};
        std::vector<bool> end_bitmask;
        std::vector<int64_t> end_to_nfa_state;
        std::unordered_map<int64_t, std::string> handler_map;

        dfa(int64_t states) : transition_table(states * BYTE_MAX, -1), end_bitmask(states), end_to_nfa_state(states, -1) {}

    public:
        auto codegen(std::ostream& out, std::string inc, std::string handle_error, std::string handle_internal_error, bool equivalence_class) const
            -> codegen_result;
        void dump(std::ostream& ofs);

        constexpr auto get_transition_table() const -> const auto& { return transition_table; }
        constexpr auto get_start_state() const -> const auto& { return start_state; }
        constexpr auto get_end_bitmask() const -> const auto& { return end_bitmask; }
        constexpr auto get_end_to_nfa_state() const -> const auto& { return end_to_nfa_state; }
    };
} // namespace lexergen
