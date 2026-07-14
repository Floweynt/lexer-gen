#include "regex.h"
#include "fwd.h"
#include "machine/equivalence_classes.h"
#include "machine/interval_set.h"
#include "machine/nfa.h"
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    template <int (*F)(int)>
    auto wrap_predicate() -> lexergen::char_set
    {
        lexergen::char_set data;

        for (int i = 0; i < lexergen::BYTE_MAX; i++)
        {
            if (F(i))
            {
                data |= lexergen::character(static_cast<char>(i));
            }
        }

        return data;
    }
} // namespace

auto lexergen::character(char ch) -> char_set { return char_set::single(static_cast<uint8_t>(ch)); }

auto lexergen::character_range(char ch_from, char ch_to) -> char_set
{ return char_set::range(static_cast<uint8_t>(ch_from), static_cast<uint8_t>(ch_to)); }

auto lexergen::codepoint_range(char_set::codepoint from, char_set::codepoint to) -> char_set { return char_set::range(from, to); }

auto lexergen::digit() -> char_set { return wrap_predicate<std::isdigit>(); }

auto lexergen::alphanumeric() -> char_set
{
    return wrap_predicate<+[](int ch) -> int { return static_cast<int>(ch == '_' || (std::isalnum(ch) != 0)); }>();
}

auto lexergen::whitespace() -> char_set { return wrap_predicate<std::isspace>(); }

auto lexergen::xdigit() -> char_set { return wrap_predicate<std::isxdigit>(); }

auto lexergen::char_regex(char_set charset) -> regex
{
    struct ch_regex : public detail::regex_element
    {
        char_set charset;
        explicit ch_regex(char_set charset) : charset(std::move(charset)) {}

        auto generate(nfa_builder& builder, int64_t& node_alloc, const equivalence_classes& classes) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            auto end = node_alloc++;

            for (const auto& iv : charset.get_intervals())
            {
                auto [first_class, last_class] = classes.class_range_for(iv.lo, iv.hi);
                for (auto class_id = first_class; class_id <= last_class; class_id++)
                {
                    builder.transition(start, end, class_id);
                }
            }

            return {start, end};
        }

        void collect_charsets(std::vector<interval_set>& out) const override { out.push_back(charset); }
    };

    return std::make_shared<ch_regex>(std::move(charset));
}

auto lexergen::string_regex(std::string str) -> regex
{
    struct str_regex : public detail::regex_element
    {
        std::string str;
        str_regex(std::string str) : str(std::move(str)) {}

        auto generate(nfa_builder& builder, int64_t& node_alloc, const equivalence_classes& classes) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            int64_t curr_target_node = start;

            for (auto ch : str)
            {
                auto tmp = node_alloc++;

                builder.transition(curr_target_node, tmp, classes.classify(static_cast<uint8_t>(ch)));
                curr_target_node = tmp;
            }

            return {start, curr_target_node};
        }

        void collect_charsets(std::vector<interval_set>& out) const override
        {
            for (auto ch : str)
            {
                out.push_back(interval_set::single(static_cast<uint8_t>(ch)));
            }
        }
    };

    return std::make_shared<str_regex>(std::move(str));
}

auto lexergen::star_regex(const regex& regexp) -> regex
{
    struct star_regex : public detail::regex_element
    {
        regex regexp;
        star_regex(regex regexp) : regexp(std::move(regexp)) {}

    protected:
        auto generate(nfa_builder& builder, int64_t& node_alloc, const equivalence_classes& classes) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            auto end = node_alloc++;
            auto [ms, me] = regexp->generate(builder, node_alloc, classes);
            builder.epsilon(ms, me);
            builder.epsilon(me, ms);
            builder.epsilon(start, ms);
            builder.epsilon(me, end);
            return {start, end};
        }

        void collect_charsets(std::vector<interval_set>& out) const override { regexp->collect_charsets(out); }
    };

    return std::make_shared<star_regex>(regexp);
};

auto lexergen::operator+(const regex& lhs, const regex& rhs) -> regex
{
    struct plus_regex : public detail::regex_element
    {
        regex lhs, rhs;
        plus_regex(regex lhs, regex rhs) : lhs(std::move(lhs)), rhs(std::move(rhs)) {}

        auto generate(nfa_builder& builder, int64_t& node_alloc, const equivalence_classes& classes) const -> std::pair<int64_t, int64_t> override
        {
            auto [ls, le] = rhs->generate(builder, node_alloc, classes);
            auto [rs, re] = lhs->generate(builder, node_alloc, classes);
            builder.epsilon(re, ls);
            return {rs, le};
        }

        void collect_charsets(std::vector<interval_set>& out) const override
        {
            lhs->collect_charsets(out);
            rhs->collect_charsets(out);
        }
    };

    return std::make_shared<plus_regex>(lhs, rhs);
}

auto lexergen::plus_regex(const regex& regexp) -> regex { return regexp + star_regex(regexp); }

auto lexergen::optional_regex(const regex& regexp) -> regex
{
    struct opt_regex : public detail::regex_element
    {
        regex regexp;
        opt_regex(regex regexp) : regexp(std::move(regexp)) {}

        auto generate(nfa_builder& builder, int64_t& node_alloc, const equivalence_classes& classes) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            auto end = node_alloc++;

            auto [ms, me] = regexp->generate(builder, node_alloc, classes);
            builder.epsilon(start, end);
            builder.epsilon(start, ms);
            builder.epsilon(me, end);
            return {start, end};
        }

        void collect_charsets(std::vector<interval_set>& out) const override { regexp->collect_charsets(out); }
    };

    return std::make_shared<opt_regex>(regexp);
}

auto lexergen::operator|(const regex& lhs, const regex& rhs) -> regex
{
    struct or_regex : public detail::regex_element
    {
        regex rhs, lhs;
        or_regex(regex rhs, regex lhs) : rhs(std::move(rhs)), lhs(std::move(lhs)) {}

        auto generate(nfa_builder& builder, int64_t& node_alloc, const equivalence_classes& classes) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            auto end = node_alloc++;

            auto [ls, le] = lhs->generate(builder, node_alloc, classes);
            auto [rs, re] = rhs->generate(builder, node_alloc, classes);

            builder.epsilon(start, ls);
            builder.epsilon(start, rs);

            builder.epsilon(le, end);
            builder.epsilon(re, end);

            return {start, end};
        }

        void collect_charsets(std::vector<interval_set>& out) const override
        {
            lhs->collect_charsets(out);
            rhs->collect_charsets(out);
        }
    };

    return std::make_shared<or_regex>(lhs, rhs);
}

auto lexergen::char_regex(char ch) -> regex { return char_regex(character(ch)); }
auto lexergen::dot_regex() -> regex { return char_regex(~empty()); }
