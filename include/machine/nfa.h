#pragma once

#include "dfa.h"
#include "machine/equivalence_classes.h"
#include <algorithm>
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
            int64_t class_id;
        };

        struct end_entry
        {
            int64_t node;
            int64_t priority;
        };

        std::vector<entry> edges;
        std::vector<std::pair<int64_t, int64_t>> epsilon_edges;
        std::vector<int64_t> start;
        std::vector<end_entry> end;
        int64_t max_val = 0;
        equivalence_classes classes;

    public:
        nfa_builder() = default;
        explicit nfa_builder(equivalence_classes classes) : classes(std::move(classes)) {}

        auto transition(int64_t from, int64_t to, int64_t class_id) -> nfa_builder&
        {
            max_val = std::max({max_val, from, to});
            edges.push_back({.from = from, .to = to, .class_id = class_id});
            return *this;
        }

        auto epsilon(int64_t from, int64_t end) -> nfa_builder&
        {
            max_val = std::max({max_val, from, end});
            epsilon_edges.emplace_back(from, end);
            return *this;
        }

        auto add_start(int64_t name) -> nfa_builder&
        {
            max_val = std::max(max_val, name);
            start.push_back(name);
            return *this;
        }

        auto add_end(int64_t name, int64_t priority = 0) -> nfa_builder&
        {
            max_val = std::max(max_val, name);
            end.push_back({.node = name, .priority = priority});
            return *this;
        }

        [[nodiscard]] auto get_classes() const -> const equivalence_classes& { return classes; }

        auto build() -> dfa;
        void dump(std::ostream& ofs);
    };
} // namespace lexergen
