// cSpell:ignore pbytes pcol pline
#include "machine/dfa.h"
#include "fwd.h"
#include "machine/cg.h"
#include "machine/data.h"
#include "machine/equivalence_classes.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

auto lexergen::dfa::source_states(int64_t class_id, const state_set& target) -> lexergen::state_set
{
    state_set split;
    const auto row_width = static_cast<int64_t>(get_class_count()) + 1;
    for (int64_t state = 0; state < get_state_count(); state++)
    {
        if (target.contains(transition_table[static_cast<std::size_t>((state * row_width) + class_id)]))
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
    const auto row_width = static_cast<int64_t>(get_class_count()) + 1;

    while (!queue.empty())
    {
        // take a from w
        auto curr = *queue.begin();
        queue.erase(queue.begin());

        for (int64_t class_id = 0; class_id < row_width; class_id++)
        {
            // let X be the set of states for which a transition on c leads to a state in A
            state_set split = source_states(class_id, curr);

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
    const auto row_width = static_cast<int64_t>(get_class_count()) + 1;
    std::vector<int64_t> new_transition_table(static_cast<std::size_t>(curr_id * row_width));

    for (int64_t new_state = 0; new_state < curr_id; new_state++)
    {
        for (int64_t class_id = 0; class_id < row_width; class_id++)
        {
            auto old_state = transition_table[static_cast<std::size_t>((new_to_old[new_state] * row_width) + class_id)];
            new_transition_table[static_cast<std::size_t>((new_state * row_width) + class_id)] = old_state == -1 ? -1 : old_to_new[old_state];
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

namespace
{
    struct dfa_view
    {
        int64_t start_state;
        std::size_t state_count;
        const std::vector<int64_t>& transition_table;
        const std::vector<bool>& end_bitmask;
        const std::vector<int64_t>& end_to_nfa_state;
        const std::unordered_map<int64_t, std::string>& handler_map;
        const lexergen::equivalence_classes& classes;
        std::string_view fn_name;
        bool emit_prelude;
    };

    using class_groups = std::unordered_map<int64_t, std::vector<int64_t>>;

    auto build_class_groups(const dfa_view& dfa, int64_t state) -> class_groups
    {
        class_groups groups;
        const auto row_width = static_cast<int64_t>(dfa.classes.class_count()) + 1;
        for (int64_t class_id = 0; class_id < row_width; class_id++)
        {
            auto target = dfa.transition_table[static_cast<std::size_t>((state * row_width) + class_id)];
            if (target != -1)
            {
                groups[target].push_back(class_id);
            }
        }
        return groups;
    }

    auto needs_unicode_decode(const dfa_view& dfa) -> bool { return dfa.classes.max_codepoint() > 0xFF; }

    constexpr std::size_t SIMD_MAX_STOP_BYTES = 8;

    auto find_simd_states(const dfa_view& dfa) -> std::unordered_map<int64_t, std::vector<char>>
    {
        std::unordered_map<int64_t, std::vector<char>> result;
        if (needs_unicode_decode(dfa))
        {
            return result;
        }

        const auto row_width = static_cast<int64_t>(dfa.classes.class_count()) + 1;

        for (int64_t state = 0; state < static_cast<int64_t>(dfa.state_count); state++)
        {
            std::vector<bool> safe(256, false);
            bool has_self_loop = false;

            for (int64_t class_id = 0; class_id < row_width; class_id++)
            {
                if (dfa.transition_table[static_cast<std::size_t>((state * row_width) + class_id)] != state)
                {
                    continue;
                }
                has_self_loop = true;

                if (class_id >= static_cast<int64_t>(dfa.classes.class_count()))
                {
                    continue;
                }

                auto interval = dfa.classes.class_interval(class_id);
                for (auto cp = interval.lo; cp <= interval.hi && cp <= 0xFF; cp++)
                {
                    safe[cp] = true;
                }
            }

            if (!has_self_loop)
            {
                continue;
            }

            std::vector<char> stops;
            for (int byte = 0; byte < 256 && stops.size() <= SIMD_MAX_STOP_BYTES; byte++)
            {
                if (!safe[static_cast<std::size_t>(byte)])
                {
                    stops.push_back(static_cast<char>(byte));
                }
            }

            if (!stops.empty() && stops.size() <= SIMD_MAX_STOP_BYTES)
            {
                result[state] = std::move(stops);
            }
        }

        return result;
    }

    void emit_simd_prelude(std::ostream& out)
    {
        out << R"cpp(#ifndef LEXGEN_SIMD_SCAN_DEFINED
#define LEXGEN_SIMD_SCAN_DEFINED
namespace lexgen_simd {

#if defined(__AVX2__)
#include <immintrin.h>
template <char... Stops>
inline std::size_t scan_safe_run(const char* p, std::size_t n)
{
    std::size_t i = 0;
    while (i + 32 <= n)
    {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p + i));
        __m256i mask = _mm256_setzero_si256();
        ((mask = _mm256_or_si256(mask, _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(Stops)))), ...);
        unsigned bits = static_cast<unsigned>(_mm256_movemask_epi8(mask));
        if (bits != 0)
        {
#if defined(_MSC_VER)
            unsigned long idx;
            _BitScanForward(&idx, bits);
            return i + idx;
#else
            return i + static_cast<std::size_t>(__builtin_ctz(bits));
#endif
        }
        i += 32;
    }
    while (i < n)
    {
        char c = p[i];
        if (((c == Stops) || ...)) { return i; }
        i++;
    }
    return i;
}
#elif defined(__SSE4_2__)
#include <nmmintrin.h>
template <char... Stops>
inline std::size_t scan_safe_run(const char* p, std::size_t n)
{
    alignas(16) static constexpr char needle[16] = {Stops...};
    constexpr int needle_len = static_cast<int>(sizeof...(Stops));
    __m128i needle_vec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needle));
    std::size_t i = 0;
    while (i + 16 <= n)
    {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
        int idx = _mm_cmpestri(needle_vec, needle_len, chunk, 16, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_LEAST_SIGNIFICANT);
        if (idx < 16) { return i + static_cast<std::size_t>(idx); }
        i += 16;
    }
    while (i < n)
    {
        char c = p[i];
        if (((c == Stops) || ...)) { return i; }
        i++;
    }
    return i;
}
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
template <char... Stops>
inline std::size_t scan_safe_run(const char* p, std::size_t n)
{
    std::size_t i = 0;
    while (i + 16 <= n)
    {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
        __m128i mask = _mm_setzero_si128();
        ((mask = _mm_or_si128(mask, _mm_cmpeq_epi8(chunk, _mm_set1_epi8(Stops)))), ...);
        unsigned bits = static_cast<unsigned>(_mm_movemask_epi8(mask));
        if (bits != 0)
        {
#if defined(_MSC_VER)
            unsigned long idx;
            _BitScanForward(&idx, bits);
            return i + idx;
#else
            return i + static_cast<std::size_t>(__builtin_ctz(bits));
#endif
        }
        i += 16;
    }
    while (i < n)
    {
        char c = p[i];
        if (((c == Stops) || ...)) { return i; }
        i++;
    }
    return i;
}
#else
template <char... Stops>
inline std::size_t scan_safe_run(const char* p, std::size_t n)
{
    std::size_t i = 0;
    while (i < n)
    {
        char c = p[i];
        if (((c == Stops) || ...)) { return i; }
        i++;
    }
    return i;
}
#endif

template <typename Source, typename = void>
struct has_bulk_scan : std::false_type {};
template <typename Source>
struct has_bulk_scan<Source, std::void_t<decltype(std::declval<Source&>().remaining()), decltype(std::declval<Source&>().skip(std::size_t{}))>>
    : std::true_type {};

} // namespace lexgen_simd
#endif

)cpp";
    }

    auto emit_c_family_classifier(std::ostream& out, const dfa_view& dfa, std::string_view peek_expr, bool is_cpp) -> std::string
    {
        const auto class_count = dfa.classes.class_count();
        const auto sentinel = static_cast<int64_t>(class_count);
        const auto prefix = std::string(dfa.fn_name) + "_";

        if (!needs_unicode_decode(dfa))
        {
            out << std::format("static const int64_t {}BYTE_CLASS[256] = {{", prefix);
            for (int cp = 0; cp < 256; cp++)
            {
                out << dfa.classes.classify(static_cast<uint32_t>(cp)) << ",";
            }
            out << "};\n\n";
            return std::format("{}BYTE_CLASS[(unsigned char){}]", prefix, peek_expr);
        }

        const auto& boundaries = dfa.classes.get_boundaries();
        out << std::format("static const uint32_t {}CLASS_BOUNDARIES[] = {{", prefix);
        for (auto b : boundaries)
        {
            out << b << "u,";
        }
        out << "};\n";
        out << std::format("static const size_t {}CLASS_BOUNDARIES_LEN = {};\n\n", prefix, boundaries.size());

        out << std::format("static int64_t {}classify_cp(uint32_t cp)\n{{\n", prefix);
        out << std::format("    size_t lo = 0, hi = {}CLASS_BOUNDARIES_LEN;\n", prefix);
        out << "    while (lo + 1 < hi)\n    {\n";
        out << "        size_t mid = lo + (hi - lo) / 2;\n";
        out << std::format("        if ({}CLASS_BOUNDARIES[mid] <= cp) lo = mid; else hi = mid;\n", prefix);
        out << "    }\n";
        out << std::format("    if (lo + 1 >= {}CLASS_BOUNDARIES_LEN) return {};\n", prefix, sentinel);
        out << "    return (int64_t)lo;\n";
        out << "}\n\n";

        out
            << (is_cpp ? std::format("template <typename Source>\nstatic uint32_t {}decode_utf8_cp(Source& src)\n{{\n", prefix)
                       : std::format("static uint32_t {}decode_utf8_cp(Source *src)\n{{\n", prefix));
        out << std::format("    uint32_t b0 = (unsigned char){};\n", peek_expr);
        out << "    if (b0 < 0x80) return b0;\n";
        out << "    int extra; uint32_t cp;\n";
        out << "    if ((b0 & 0xE0) == 0xC0) { extra = 1; cp = b0 & 0x1F; }\n";
        out << "    else if ((b0 & 0xF0) == 0xE0) { extra = 2; cp = b0 & 0x0F; }\n";
        out << "    else if ((b0 & 0xF8) == 0xF0) { extra = 3; cp = b0 & 0x07; }\n";
        out << "    else return 0xFFFD;\n";
        out << "    for (int i = 0; i < extra; i++)\n    {\n";
        out << std::format("        uint32_t bn = (unsigned char){};\n", peek_expr);
        out << "        if ((bn & 0xC0) != 0x80) return 0xFFFD;\n";
        out << "        cp = (cp << 6) | (bn & 0x3F);\n";
        out << "    }\n";
        out << "    return cp;\n";
        out << "}\n\n";

        return std::format("{}classify_cp({}decode_utf8_cp(src))", prefix, prefix);
    }

    auto emit_cpp(
        std::ostream& out, const dfa_view& dfa, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error,
        bool enable_simd
    ) -> lexergen::codegen_result
    {
        if (dfa.emit_prelude)
        {
            out << "#include <cstdint>\n#include <cstddef>\n#include <string_view>\n\n";
            if (enable_simd)
            {
                out << "#include <type_traits>\n#include <utility>\n\n";
            }
            out << inc << "\n\n";
        }

        auto simd_states = enable_simd ? find_simd_states(dfa) : std::unordered_map<int64_t, std::vector<char>>{};
        if (dfa.emit_prelude && enable_simd)
        {
            emit_simd_prelude(out);
        }

        auto class_expr = emit_c_family_classifier(out, dfa, "src.peek()", true);

        out << "template <typename Source, typename Ctx>\n";
        out << std::format("[[gnu::always_inline]] inline auto {}(Source& src, Ctx& ctx)\n{{\n", dfa.fn_name);
        out << "    (void)ctx;\n";
        out << "    int64_t latest_match = -1;\n";
        out << "\n    src.start_token();\n";
        out << "    [[maybe_unused]] std::size_t start_bytes = src.bytes();\n\n";
        out << std::format("    goto STATE_{};\n\n", dfa.start_state);

        std::size_t total_cases = 0;

        for (int64_t state = 0; state < static_cast<int64_t>(dfa.state_count); state++)
        {
            out << std::format("STATE_{}:\n", state);

            if (auto simd_it = simd_states.find(state); simd_it != simd_states.end())
            {
                out << "    if constexpr (lexgen_simd::has_bulk_scan<Source>::value)\n    {\n";
                out << "        auto simd_span = src.remaining();\n";
                out << "        auto simd_n = lexgen_simd::scan_safe_run<";
                for (std::size_t i = 0; i < simd_it->second.size(); i++)
                {
                    if (i != 0)
                    {
                        out << ", ";
                    }
                    out << "(char)" << static_cast<int>(static_cast<unsigned char>(simd_it->second[i]));
                }
                out << ">(simd_span.data(), simd_span.size());\n";
                out << "        if (simd_n > 0) { src.skip(simd_n); }\n    }\n";
            }

            if (dfa.end_bitmask[state])
            {
                out << std::format("    latest_match = {};\n    src.accept();\n", dfa.end_to_nfa_state[state]);
            }

            auto groups = build_class_groups(dfa, state);
            if (groups.empty())
            {
                out << "    goto FAIL;\n\n";
                continue;
            }

            out << std::format("    switch ({})\n    {{\n", class_expr);
            for (const auto& [target, class_ids] : groups)
            {
                for (auto class_id : class_ids)
                {
                    out << std::format("    case {}: ", class_id);
                    total_cases++;
                }
                out << std::format("goto STATE_{};\n", target);
            }
            out << "    default: goto FAIL;\n    }\n\n";
        }

        out << "FAIL:\n";
        out << "    if (latest_match == -1)\n    {\n";
        out << "        " << handle_error << "\n";
        out << "    }\n\n";
        out << "    src.backtrack();\n";
        out << "    {\n";
        out << "        [[maybe_unused]] std::string_view buffer = src.text();\n";
        out << "        switch (latest_match)\n        {\n";

        for (const auto& [nfa_state, handler] : dfa.handler_map)
        {
            out << std::format("        case {}: {}\n", nfa_state, handler);
        }

        out << "        default:\n            " << handle_internal_error << "\n";
        out << "        }\n    }\n\n";

        out << "    latest_match = -1;\n";
        out << "    src.start_token();\n";
        out << "    start_bytes = src.bytes();\n";
        out << std::format("    goto STATE_{};\n", dfa.start_state);
        out << "}\n";

        return {.state_count = dfa.state_count, .case_count = total_cases};
    }

    void emit_simd_scan_fn_c(std::ostream& out, const std::string& name, const std::vector<char>& stops)
    {
        std::string mask_lines;
        for (auto stop : stops)
        {
            mask_lines += std::format(
                "        mask = _mm_or_si128(mask, _mm_cmpeq_epi8(chunk, _mm_set1_epi8((char){})));\n", static_cast<int>(static_cast<unsigned char>(stop))
            );
        }

        std::string cmp_chain;
        for (std::size_t i = 0; i < stops.size(); i++)
        {
            cmp_chain += std::format("{}c == (char){}", i == 0 ? "" : " || ", static_cast<int>(static_cast<unsigned char>(stops[i])));
        }

        out << std::format(
            R"c(#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
static size_t {0}(const char* p, size_t n)
{{
    size_t i = 0;
    while (i + 16 <= n)
    {{
        __m128i chunk = _mm_loadu_si128((const __m128i*)(p + i));
        __m128i mask = _mm_setzero_si128();
{1}        {{
        unsigned bits = (unsigned)_mm_movemask_epi8(mask);
        if (bits != 0)
        {{
#if defined(_MSC_VER)
            unsigned long idx; _BitScanForward(&idx, bits); return i + idx;
#else
            return i + (size_t)__builtin_ctz(bits);
#endif
        }}
        }}
        i += 16;
    }}
    while (i < n)
    {{
        char c = p[i];
        if ({2}) {{ return i; }}
        i++;
    }}
    return i;
}}
#else
static size_t {0}(const char* p, size_t n)
{{
    size_t i = 0;
    while (i < n)
    {{
        char c = p[i];
        if ({2}) {{ return i; }}
        i++;
    }}
    return i;
}}
#endif

)c",
            name, mask_lines, cmp_chain
        );
    }

    auto emit_c(
        std::ostream& out, const dfa_view& dfa, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error,
        bool enable_simd
    ) -> lexergen::codegen_result
    {
        if (dfa.emit_prelude)
        {
            out << "#include <stdint.h>\n#include <stddef.h>\n\n";
            out << "typedef struct { const char *ptr; size_t len; } lex_text;\n\n";
            out << inc << "\n\n";
            out << "#ifndef LEX_RESULT_TYPE\n#define LEX_RESULT_TYPE int\n#endif\n\n";
            out << "#if defined(__GNUC__) || defined(__clang__)\n#define LEXGEN_ALWAYS_INLINE __attribute__((always_inline)) inline\n#else\n#define "
                   "LEXGEN_ALWAYS_INLINE inline\n#endif\n\n";
        }

        auto simd_states = enable_simd ? find_simd_states(dfa) : std::unordered_map<int64_t, std::vector<char>>{};
        std::unordered_map<int64_t, std::string> simd_fn_names;
        if (enable_simd && !simd_states.empty())
        {
            const auto prefix = std::string(dfa.fn_name) + "_";
            int64_t idx = 0;
            for (const auto& [state, stops] : simd_states)
            {
                auto name = std::format("{}scan_safe_run_{}", prefix, idx++);
                emit_simd_scan_fn_c(out, name, stops);
                simd_fn_names[state] = name;
            }
        }

        auto class_expr = emit_c_family_classifier(out, dfa, "Source_peek(src)", false);

        out << std::format("LEXGEN_ALWAYS_INLINE LEX_RESULT_TYPE {}(Source *src, Ctx *ctx)\n{{\n", dfa.fn_name);
        out << "    (void)ctx;\n";
        out << "    int64_t latest_match = -1;\n";
        out << "\n    Source_start_token(src);\n";
        out << "    size_t start_bytes = Source_bytes(src);\n";
        out << "    (void)start_bytes;\n\n";
        out << std::format("    goto STATE_{};\n\n", dfa.start_state);

        std::size_t total_cases = 0;

        for (int64_t state = 0; state < static_cast<int64_t>(dfa.state_count); state++)
        {
            out << std::format("STATE_{}:\n", state);

            if (auto simd_it = simd_fn_names.find(state); simd_it != simd_fn_names.end())
            {
                out << "#ifdef LEXGEN_C_SOURCE_HAS_SCAN\n    {\n";
                out << "        lex_text simd_span = Source_remaining(src);\n";
                out << std::format("        size_t simd_n = {}(simd_span.ptr, simd_span.len);\n", simd_it->second);
                out << "        if (simd_n > 0) { Source_skip(src, simd_n); }\n    }\n#endif\n";
            }

            if (dfa.end_bitmask[state])
            {
                out << std::format("    latest_match = {};\n    Source_accept(src);\n", dfa.end_to_nfa_state[state]);
            }

            auto groups = build_class_groups(dfa, state);
            if (groups.empty())
            {
                out << "    goto FAIL;\n\n";
                continue;
            }

            out << std::format("    switch ({})\n    {{\n", class_expr);
            for (const auto& [target, class_ids] : groups)
            {
                for (auto class_id : class_ids)
                {
                    out << std::format("    case {}: ", class_id);
                    total_cases++;
                }
                out << std::format("goto STATE_{};\n", target);
            }
            out << "    default: goto FAIL;\n    }\n\n";
        }

        out << "FAIL:\n";
        out << "    if (latest_match == -1)\n    {\n";
        out << "        " << handle_error << "\n";
        out << "    }\n\n";
        out << "    Source_backtrack(src);\n";
        out << "    {\n";
        out << "        lex_text buffer = Source_text(src);\n";
        out << "        (void)buffer;\n";
        out << "        switch (latest_match)\n        {\n";

        for (const auto& [nfa_state, handler] : dfa.handler_map)
        {
            out << std::format("        case {}: {}\n", nfa_state, handler);
        }

        out << "        default:\n            " << handle_internal_error << "\n";
        out << "        }\n    }\n\n";

        out << "    latest_match = -1;\n";
        out << "    Source_start_token(src);\n";
        out << "    start_bytes = Source_bytes(src);\n";
        out << std::format("    goto STATE_{};\n", dfa.start_state);
        out << "}\n";

        return {.state_count = dfa.state_count, .case_count = total_cases};
    }

    auto emit_js_family_classifier(std::ostream& out, const dfa_view& dfa, bool is_java) -> std::string
    {
        const auto class_count = dfa.classes.class_count();
        const auto sentinel = static_cast<int64_t>(class_count);
        const char* int_arr = is_java ? "int[]" : "";
        const char* decl = is_java ? "static final " : "const ";

        const char* open = is_java ? "{" : "[";
        const char* close = is_java ? "}" : "]";
        const auto prefix = std::string(dfa.fn_name) + "_";

        if (!needs_unicode_decode(dfa))
        {
            out << decl << int_arr << std::format(" {}BYTE_CLASS = ", prefix) << (is_java ? "new int[] " : "") << open;
            for (int cp = 0; cp < 256; cp++)
            {
                out << dfa.classes.classify(static_cast<uint32_t>(cp)) << ",";
            }
            out << close << ";\n\n";
            return std::format("{}BYTE_CLASS[src.peek()]", prefix);
        }

        const auto& boundaries = dfa.classes.get_boundaries();
        out << decl << (is_java ? "int[]" : "") << std::format(" {}CLASS_BOUNDARIES = ", prefix) << (is_java ? "new int[] " : "") << open;
        for (auto bound : boundaries)
        {
            out << bound << ",";
        }
        out << close << ";\n\n";

        if (is_java)
        {
            out << std::format("static int {}classifyCp(int cp)\n{{\n", prefix);
            out << std::format("    int lo = 0, hi = {}CLASS_BOUNDARIES.length;\n", prefix);
            out << std::format(
                "    while (lo + 1 < hi) {{ int mid = lo + (hi - lo) / 2; if ({}CLASS_BOUNDARIES[mid] <= cp) lo = mid; else hi = mid; }}\n", prefix
            );
            out << std::format("    if (lo + 1 >= {}CLASS_BOUNDARIES.length) return {};\n", prefix, sentinel);
            out << "    return lo;\n";
            out << "}\n\n";
            out << std::format("static int {}decodeUtf8Cp(Source src)\n{{\n", prefix);
            out << "    int b0 = src.peek();\n";
            out << "    if (b0 < 0x80) return b0;\n";
            out << "    int extra; int cp;\n";
            out << "    if ((b0 & 0xE0) == 0xC0) { extra = 1; cp = b0 & 0x1F; }\n";
            out << "    else if ((b0 & 0xF0) == 0xE0) { extra = 2; cp = b0 & 0x0F; }\n";
            out << "    else if ((b0 & 0xF8) == 0xF0) { extra = 3; cp = b0 & 0x07; }\n";
            out << "    else return 0xFFFD;\n";
            out << "    for (int i = 0; i < extra; i++) { int bn = src.peek(); if ((bn & 0xC0) != 0x80) return 0xFFFD; cp = (cp << 6) | (bn & 0x3F); "
                   "}\n";
            out << "    return cp;\n";
            out << "}\n\n";
            return std::format("{}classifyCp({}decodeUtf8Cp(src))", prefix, prefix);
        }

        out << std::format("function {}classifyCp(cp)\n{{\n", prefix);
        out << std::format("    let lo = 0, hi = {}CLASS_BOUNDARIES.length;\n", prefix);
        out << std::format(
            "    while (lo + 1 < hi) {{ const mid = lo + Math.floor((hi - lo) / 2); if ({}CLASS_BOUNDARIES[mid] <= cp) lo = mid; else hi = mid; "
            "}}\n",
            prefix
        );
        out << std::format("    if (lo + 1 >= {}CLASS_BOUNDARIES.length) return {};\n", prefix, sentinel);
        out << "    return lo;\n";
        out << "}\n\n";
        out << std::format("function {}decodeUtf8Cp(src)\n{{\n", prefix);
        out << "    const b0 = src.peek();\n";
        out << "    if (b0 < 0x80) return b0;\n";
        out << "    let extra, cp;\n";
        out << "    if ((b0 & 0xE0) === 0xC0) { extra = 1; cp = b0 & 0x1F; }\n";
        out << "    else if ((b0 & 0xF0) === 0xE0) { extra = 2; cp = b0 & 0x0F; }\n";
        out << "    else if ((b0 & 0xF8) === 0xF0) { extra = 3; cp = b0 & 0x07; }\n";
        out << "    else return 0xFFFD;\n";
        out << "    for (let i = 0; i < extra; i++) { const bn = src.peek(); if ((bn & 0xC0) !== 0x80) return 0xFFFD; cp = (cp << 6) | (bn & 0x3F); "
               "}\n";
        out << "    return cp;\n";
        out << "}\n\n";
        return std::format("{}classifyCp({}decodeUtf8Cp(src))", prefix, prefix);
    }

    auto emit_switch_loop(
        std::ostream& out, const dfa_view& dfa, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error,
        bool is_java
    ) -> lexergen::codegen_result
    {
        if (dfa.emit_prelude)
        {
            out << inc << "\n\n";
        }
        auto class_expr = emit_js_family_classifier(out, dfa, is_java);

        if (is_java)
        {
            out << std::format("static Object {}(Source src, Ctx ctx)\n{{\n", dfa.fn_name);
            out << "    int latestMatch = -1;\n";
            out << "    int state = " << dfa.start_state << ";\n\n";
            out << "    src.startToken();\n";
            out << "    long startBytes = src.bytes();\n\n";
        }
        else
        {
            out << std::format("function {}(src, ctx)\n{{\n", dfa.fn_name);
            out << "    let latestMatch = -1;\n";
            out << "    let state = " << dfa.start_state << ";\n\n";
            out << "    src.startToken();\n";
            out << "    let startBytes = src.bytes();\n\n";
        }

        out << "    while (true) {\n    switch (state) {\n";

        std::size_t total_cases = 0;

        for (int64_t state = 0; state < static_cast<int64_t>(dfa.state_count); state++)
        {
            out << std::format("    case {}: {{\n", state);

            if (dfa.end_bitmask[state])
            {
                out << std::format("        latestMatch = {};\n        src.accept();\n", dfa.end_to_nfa_state[state]);
            }

            auto groups = build_class_groups(dfa, state);
            if (groups.empty())
            {
                out << "        state = -1; continue;\n    }\n";
                continue;
            }

            out << std::format("        switch ({}) {{\n", class_expr);
            for (const auto& [target, class_ids] : groups)
            {
                for (auto class_id : class_ids)
                {
                    out << std::format("        case {}: ", class_id);
                    total_cases++;
                }
                out << std::format("state = {}; continue;\n", target);
            }
            out << "        default: state = -1; continue;\n        }\n    }\n";
        }

        out << "    case -1: {\n";
        out << "        if (latestMatch == -1) {\n";
        out << "            " << handle_error << "\n";
        out << "        }\n\n";
        out << "        src.backtrack();\n";
        out << (is_java ? "        String buffer = src.text();\n" : "        const buffer = src.text();\n");
        out << "        switch (latestMatch) {\n";

        for (const auto& [nfa_state, handler] : dfa.handler_map)
        {
            out << std::format("        case {}: {}\n", nfa_state, handler);
        }

        out << "        default:\n            " << handle_internal_error << "\n";
        out << "        }\n\n";

        out << "        latestMatch = -1;\n";
        out << "        src.startToken();\n";
        out << "        startBytes = src.bytes();\n";
        out << std::format("        state = {};\n", dfa.start_state);
        out << "        continue;\n    }\n";
        out << "    }\n    }\n}\n";

        return {.state_count = dfa.state_count, .case_count = total_cases};
    }

    auto describe_cp_plain(uint32_t cp) -> std::string
    {
        switch (cp)
        {
        case '\n':
            return "\\n";
        case '\r':
            return "\\r";
        case '\t':
            return "\\t";
        case '\0':
            return "\\0";
        case '\\':
            return "\\\\";
        default:
            break;
        }

        if (cp >= 0x20 && cp < 0x7F)
        {
            return std::string(1, static_cast<char>(cp));
        }
        if (cp <= 0xFF)
        {
            return std::format("\\x{:02x}", cp);
        }
        return std::format("U+{:04X}", cp);
    }

    auto describe_interval_plain(lexergen::interval_set::codepoint lo, lexergen::interval_set::codepoint hi) -> std::string
    {
        if (lo == hi)
        {
            return describe_cp_plain(lo);
        }
        return std::format("{}-{}", describe_cp_plain(lo), describe_cp_plain(hi));
    }

} // namespace

auto lexergen::dfa::codegen(
    std::ostream& out, const std::string& inc, const std::string& handle_error, const std::string& handle_internal_error, target_lang lang,
    std::string_view fn_name, bool emit_prelude, bool enable_simd
) const -> codegen_result
{
    const dfa_view view{
        .start_state = start_state,
        .state_count = static_cast<std::size_t>(get_state_count()),
        .transition_table = transition_table,
        .end_bitmask = end_bitmask,
        .end_to_nfa_state = end_to_nfa_state,
        .handler_map = handler_map,
        .classes = classes,
        .fn_name = fn_name.empty() ? base_fn_name(lang) : fn_name,
        .emit_prelude = emit_prelude,
    };

    switch (lang)
    {
    case target_lang::CPP:
        return emit_cpp(out, view, inc, handle_error, handle_internal_error, enable_simd);
    case target_lang::C:
        return emit_c(out, view, inc, handle_error, handle_internal_error, enable_simd);
    case target_lang::JAVA:
        return emit_switch_loop(out, view, inc, handle_error, handle_internal_error, true);
    case target_lang::JS:
        return emit_switch_loop(out, view, inc, handle_error, handle_internal_error, false);
    }

    return {.state_count = 0, .case_count = 0};
}

auto lexergen::dfa::analyze_warnings(bool check_unmatchable, bool check_past_end) const -> dfa_warnings
{
    dfa_warnings result;
    const auto state_count = get_state_count();
    const auto row_width = static_cast<int64_t>(classes.class_count()) + 1;

    if (check_unmatchable)
    {
        std::vector<bool> unconfirmed(static_cast<std::size_t>(state_count), false);
        std::vector<int64_t> queue{start_state};
        unconfirmed[static_cast<std::size_t>(start_state)] = true;

        for (std::size_t qi = 0; qi < queue.size(); qi++)
        {
            auto s = queue[qi];
            if (end_bitmask[static_cast<std::size_t>(s)])
            {
                continue;
            }

            for (int64_t c = 0; c < row_width; c++)
            {
                auto t = transition_table[static_cast<std::size_t>((s * row_width) + c)];
                if (t == -1 || unconfirmed[static_cast<std::size_t>(t)])
                {
                    continue;
                }
                unconfirmed[static_cast<std::size_t>(t)] = true;
                queue.push_back(t);
            }
        }

        for (int64_t s = 0; s < state_count; s++)
        {
            if (!unconfirmed[static_cast<std::size_t>(s)] || end_bitmask[static_cast<std::size_t>(s)])
            {
                continue;
            }

            for (int64_t c = 0; c < row_width; c++)
            {
                if (transition_table[static_cast<std::size_t>((s * row_width) + c)] != -1)
                {
                    continue;
                }

                std::string detail;
                if (c < static_cast<int64_t>(classes.class_count()))
                {
                    auto iv = classes.class_interval(c);
                    detail = std::format("state {} can fail to match on '{}' with no token matched yet", s, describe_interval_plain(iv.lo, iv.hi));
                }
                else
                {
                    detail = std::format("state {} can fail to match on out-of-range input with no token matched yet", s);
                }
                result.unmatchable.push_back({.state = s, .detail = std::move(detail)});
                break;
            }
        }
    }

    if (check_past_end)
    {
        auto zero_class = classes.classify(0);
        for (int64_t s = 0; s < state_count; s++)
        {
            auto t = transition_table[static_cast<std::size_t>((s * row_width) + zero_class)];
            if (t == -1)
            {
                continue;
            }

            bool target_peeks_again = false;
            for (int64_t c = 0; c < row_width; c++)
            {
                if (transition_table[static_cast<std::size_t>((t * row_width) + c)] != -1)
                {
                    target_peeks_again = true;
                    break;
                }
            }

            if (!target_peeks_again)
            {
                continue;
            }

            result.past_the_end.push_back(
                {.state = s,
                 .detail = std::format("state {} consumes '\\0' but doesn't stop there, landing in state {} which peeks further input", s, t)}
            );
        }
    }

    return result;
}
