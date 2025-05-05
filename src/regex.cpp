#include "regex.h"
#include "fwd.h"
#include "machine/nfa.h"
#include <bitset>
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

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
                data.set(i);
            }
        }

        return data;
    }
} // namespace

auto lexergen::character(char ch) -> char_set { return char_set().set(ch); }

auto lexergen::character_range(char ch_from, char ch_to) -> char_set
{
    char_set data;

    for (uint8_t i = ch_from; i <= (uint8_t)ch_to; i++)
    {
        data.set(i);
    }

    return data;
}

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
        std::bitset<BYTE_MAX> charset;

        auto generate(nfa_builder& builder, int64_t& node_alloc) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            auto end = node_alloc++;

            for (int64_t i = 0; i < BYTE_MAX; i++)
            {
                if (charset.test(i))
                {
                    builder.transition(start, end, (char)i);
                }
            }

            return {start, end};
        }
    };

    auto regexp = std::make_shared<ch_regex>();
    for (int64_t i = 0; i < BYTE_MAX; i++)
    {
        regexp->charset.set(i, charset.test(i));
    }
    return regexp;
}

auto lexergen::string_regex(std::string str) -> regex
{
    struct str_regex : public detail::regex_element
    {
        std::string str;
        str_regex(std::string str) : str(std::move(str)) {}

        auto generate(nfa_builder& builder, int64_t& node_alloc) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            int64_t curr_target_node = start;

            for (auto ch : str)
            {
                auto tmp = node_alloc++;

                builder.transition(curr_target_node, tmp, ch);
                curr_target_node = tmp;
            }

            return {start, curr_target_node};
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
        auto generate(nfa_builder& builder, int64_t& node_alloc) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            auto end = node_alloc++;
            auto [ms, me] = regexp->generate(builder, node_alloc);
            builder.epsilon(ms, me);
            builder.epsilon(me, ms);
            builder.epsilon(start, ms);
            builder.epsilon(me, end);
            return {start, end};
        }
    };

    return std::make_shared<star_regex>(regexp);
};

auto lexergen::operator+(const regex& lhs, const regex& rhs) -> regex
{
    struct plus_regex : public detail::regex_element
    {
        regex lhs, rhs;
        plus_regex(regex lhs, regex rhs) : lhs(std::move(lhs)), rhs(std::move(rhs)) {}

        auto generate(nfa_builder& builder, int64_t& node_alloc) const -> std::pair<int64_t, int64_t> override
        {
            auto [ls, le] = rhs->generate(builder, node_alloc);
            auto [rs, re] = lhs->generate(builder, node_alloc);
            builder.epsilon(re, ls);
            return {rs, le};
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

        auto generate(nfa_builder& builder, int64_t& node_alloc) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            auto end = node_alloc++;

            auto [ms, me] = regexp->generate(builder, node_alloc);
            builder.epsilon(start, end);
            builder.epsilon(start, ms);
            builder.epsilon(me, end);
            return {start, end};
        }
    };

    return std::make_shared<opt_regex>(regexp);
}

auto lexergen::operator|(const regex& lhs, const regex& rhs) -> regex
{
    struct or_regex : public detail::regex_element
    {
        regex rhs, lhs;
        or_regex(regex rhs, regex lhs) : rhs(std::move(rhs)), lhs(std::move(lhs)) {}

        auto generate(nfa_builder& builder, int64_t& node_alloc) const -> std::pair<int64_t, int64_t> override
        {
            auto start = node_alloc++;
            auto end = node_alloc++;

            auto [ls, le] = lhs->generate(builder, node_alloc);
            auto [rs, re] = rhs->generate(builder, node_alloc);

            builder.epsilon(start, ls);
            builder.epsilon(start, rs);

            builder.epsilon(le, end);
            builder.epsilon(re, end);

            return {start, end};
        }
    };

    return std::make_shared<or_regex>(lhs, rhs);
}

auto lexergen::char_regex(char ch) -> regex { return char_regex(character(ch)); }
auto lexergen::dot_regex() -> regex { return char_regex(~empty()); }
