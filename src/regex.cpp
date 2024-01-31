#pragma once

#include <bitset>
#include <memory>
#include <regex.h>
#include <machine/nfa.h>

namespace lexergen
{
    auto character(char ch) -> char_set
    {
        class character : public detail::character_set
        {
            char ch;

        public:
            character(char ch) : ch(ch) {}
            [[nodiscard]] auto test(char ch) const -> bool override { return ch == this->ch; }
            ~character() override = default;
        };

        return std::make_shared<character>(ch);
    }

    auto character_range(char ch_from, char ch_to) -> char_set
    {
        class character_range : public detail::character_set
        {
            char from;
            char to;

        public:
            character_range(char ch_from, char ch_to) : from(ch_from), to(ch_to) {}
            [[nodiscard]] auto test(char ch) const -> bool override { return from <= ch && ch <= to; }
            ~character_range() override = default;
        };

        return std::make_shared<character_range>(ch_from, ch_to);
    }

    template <int (*F)(int)>
    auto wrap_predicate() -> const char_set&
    {
        class wrap_libc : public detail::character_set
        {
        public:
            [[nodiscard]] auto test(char ch) const -> bool override { return F(ch); }
            ~wrap_libc() override = default;
        };

        static char_set instance = std::make_shared<wrap_libc>();
        return instance;
    }

    auto digit() -> const char_set& { return wrap_predicate<std::isdigit>(); }

    auto alphanumeric() -> const char_set&
    {
        return wrap_predicate<+[](int ch) -> int { return static_cast<int>(ch == '_' || (std::isalnum(ch) != 0)); }>();
    }

    auto whitespace() -> const char_set& { return wrap_predicate<std::isspace>(); }

    auto empty() -> const char_set&
    {
        return wrap_predicate<+[](int) -> int { return 0; }>();
    }

    auto xdigit() -> const char_set& { return wrap_predicate<isxdigit>(); }

    auto operator&&(const char_set& lhs, const char_set& rhs) -> char_set
    {
        class character_and : public detail::character_set
        {
            char_set lhs, rhs;

        public:
            character_and(char_set lhs, char_set rhs) : lhs(std::move(lhs)), rhs(std::move(rhs)) {}
            [[nodiscard]] auto test(char ch) const -> bool override { return rhs->test(ch) && lhs->test(ch); }
            ~character_and() override = default;
        };
        return std::make_shared<character_and>(lhs, rhs);
    }

    auto operator||(const char_set& lhs, const char_set& rhs) -> char_set
    {
        class character_or : public detail::character_set
        {
            char_set lhs, rhs;

        public:
            character_or(char_set lhs, char_set rhs) : lhs(std::move(lhs)), rhs(std::move(rhs)) {}
            [[nodiscard]] auto test(char ch) const -> bool override { return rhs->test(ch) || lhs->test(ch); }
            ~character_or() override = default;
        };
        return std::make_shared<character_or>(lhs, rhs);
    }

    auto operator!(const char_set& lhs) -> char_set
    {
        class character_not : public detail::character_set
        {
            char_set set;

        public:
            character_not(char_set set) : set(std::move(set)) {}
            [[nodiscard]] auto test(char ch) const -> bool override { return !set->test(ch); }
            ~character_not() override = default;
        };
        return std::make_shared<character_not>(lhs);
    }

    auto char_regex(const char_set& charset) -> regex
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
            regexp->charset.set(i, charset->test((char)i));
        }
        return regexp;
    }

    auto string_regex(const std::string& str) -> regex
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

        return std::make_shared<str_regex>(str);
    }

    auto star_regex(const regex& regexp) -> regex
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

    auto operator+(const regex& lhs, const regex& rhs) -> regex
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

    auto plus_regex(const regex& regexp) -> regex { return regexp + star_regex(regexp); }

    auto optional_regex(const regex& regexp) -> regex
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

    auto operator|(const regex& lhs, const regex& rhs) -> regex
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

    auto char_regex(char ch) -> regex { return char_regex(character(ch)); }
    auto dot_regex() -> regex { return char_regex(!empty()); }
} // namespace lexergen
