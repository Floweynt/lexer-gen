// cSpell:ignore pbytes pcol pline
#include "machine/dfa.h"
#include "fwd.h"
#include "machine/cg.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <unordered_map>
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
} // namespace

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
