#pragma once

#include "fwd.h"
#include <bitset>
#include <climits>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace lexergen
{
    namespace detail
    {
        class regex_element
        {
        public:
            virtual auto generate(lexergen::nfa_builder& builder, int64_t& node_alloc) const -> std::pair<int64_t, int64_t> = 0;
            virtual ~regex_element() = default;
        };
    } // namespace detail

    using char_set = std::bitset<1 << CHAR_BIT>;
    using regex = std::shared_ptr<detail::regex_element>;

    struct regex_parse_result
    {
        bool is_success;
        regex expr;
        std::string error_msg;
        std::string rest;
    };

    auto parse_regex(const std::string& str) -> regex_parse_result;

    auto character(char ch) -> char_set;
    auto character_range(char ch_from, char ch_to) -> char_set;
    auto digit() -> char_set;
    auto alphanumeric() -> char_set;
    auto whitespace() -> char_set;
    [[nodiscard]] constexpr auto empty() -> char_set { return {}; }
    auto xdigit() -> char_set;

    auto char_regex(char_set charset) -> regex;
    auto string_regex( std::string str) -> regex;
    auto star_regex(const regex& regexp) -> regex;
    auto operator+(const regex& lhs, const regex& rhs) -> regex;
    auto plus_regex(const regex& regexp) -> regex;
    auto optional_regex(const regex& regexp) -> regex;
    auto operator|(const regex& lhs, const regex& rhs) -> regex;
    auto char_regex(char ch) -> regex;
    auto dot_regex() -> regex;
} // namespace lexergen
