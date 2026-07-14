#include "machine/dfa.h"
#include "machine/equivalence_classes.h"
#include "machine/interval_set.h"
#include "machine/nfa.h"
#include "regex.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

auto lexergen::make_lexer(const std::vector<std::pair<regex, std::string>>& table) -> std::pair<dfa, nfa_builder>
{
    std::vector<interval_set> charsets;
    for (const auto& entry : table)
    {
        entry.first->collect_charsets(charsets);
    }

    nfa_builder nfa(equivalence_classes::build(charsets));
    int64_t node_alloc = 0;
    int64_t start = node_alloc++;
    std::unordered_map<int64_t, std::string> handler_map;

    for (const auto& entry : table)
    {
        auto [s, e] = entry.first->generate(nfa, node_alloc, nfa.get_classes());
        nfa.epsilon(start, s);
        nfa.add_end(e);
        handler_map[e] = entry.second;
    }

    nfa.add_start(start);
    auto dfa = nfa.build();
    dfa.handler_map = std::move(handler_map);
    return {dfa, nfa};
}
