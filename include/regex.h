#pragma once

#include "fwd.h"
#include <memory>
#include <vector>

namespace lexergen
{
    namespace detail
    {
        class character_set
        {
        public:
            [[nodiscard]] virtual auto test(char ch) const -> bool = 0;
            virtual ~character_set() = default;
        };

        class regex_element
        {
        public:
            virtual auto generate(lexergen::nfa_builder& builder, int64_t& node_alloc) const -> std::pair<int64_t, int64_t> = 0;
            virtual ~regex_element() = default;
        };
    } // namespace detail

    using char_set = std::shared_ptr<detail::character_set>;
    using regex = std::shared_ptr<detail::regex_element>;

    class regex_parser
    {
    public:
        regex_parser(std::string regex) : curr_regex(std::move(regex)) {}
        auto parse() -> regex;
        void dump_error();

        [[nodiscard]] constexpr auto has_errors() const { return !errors.empty(); }

    private:
        [[nodiscard]] auto peek() const -> char;
        [[nodiscard]] auto eof() const -> bool;
        auto match(char ch) -> bool;
        auto parse_char() -> regex;
        auto parse_atom() -> regex;
        auto parse_class() -> regex;
        auto parse_capture() -> regex;
        auto parse_escape() -> std::string;
        auto next() -> char;
        auto parse_quantifier() -> regex;
        auto parse_concat() -> regex;
        auto parse_alt() -> regex;
        auto parse_name() -> std::string;

        auto parse_char_class(const std::string& str, bool negate) -> regex;
        void error(std::string msg);

        const std::string curr_regex;
        std::vector<std::string> errors;
        size_t curr_pos{};
    };

    auto character(char ch) -> char_set;
    auto character_range(char ch_from, char ch_to) -> char_set;
    auto digit() -> const char_set&;
    auto alphanumeric() -> const char_set&;
    auto whitespace() -> const char_set&;
    auto empty() -> const char_set&;
    auto xdigit() -> const char_set&;
    auto operator&&(const char_set& lhs, const char_set& rhs) -> char_set;
    auto operator||(const char_set& lhs, const char_set& rhs) -> char_set;
    auto operator!(const char_set& lhs) -> char_set;

    auto char_regex(const char_set& charset) -> regex;
    auto string_regex(const std::string& str) -> regex;
    auto star_regex(const regex& regexp) -> regex;
    auto operator+(const regex& lhs, const regex& rhs) -> regex;
    auto plus_regex(const regex& regexp) -> regex;
    auto optional_regex(const regex& regexp) -> regex;
    auto operator|(const regex& lhs, const regex& rhs) -> regex;
    auto char_regex(char ch) -> regex;
    auto dot_regex() -> regex;
} // namespace lexergen
