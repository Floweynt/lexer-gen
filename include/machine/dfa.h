#pragma once

#include "fwd.h"
#include "machine/cg.h"
#include "machine/data.h"
#include "regex.h"
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lexergen
{
    auto make_lexer(const std::vector<std::pair<regex, std::string>>& table) -> std::pair<dfa, nfa_builder>;

    struct codegen_result
    {
        std::size_t state_count;
        std::size_t case_count;
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

        auto source_states(size_t ch, const state_set& target) -> state_set;
        auto hopcroft(const std::unordered_set<state_set>& initial) -> std::vector<state_set>;
        void reconstruct(const std::vector<state_set>& partitions);

    public:
        void optimize(bool debug);
        auto codegen(
            std::ostream& out, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error, target_lang lang
        ) const -> codegen_result;
        void dump(std::ostream& ofs);

        constexpr auto get_transition_table() const -> const auto& { return transition_table; }
        constexpr auto get_start_state() const -> const auto& { return start_state; }
        constexpr auto get_end_bitmask() const -> const auto& { return end_bitmask; }
        constexpr auto get_end_to_nfa_state() const -> const auto& { return end_to_nfa_state; }
        constexpr auto get_state_count() const -> auto { return transition_table.size() / BYTE_MAX; }
    };
} // namespace lexergen
