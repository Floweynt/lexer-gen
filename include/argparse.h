#pragma once

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lexergen
{
    struct option
    {
        std::string_view name;
        std::string_view long_flag;
        std::string_view short_flag;
        std::string_view description;
        bool has_args;
        bool required;
    };

    struct argument_value
    {
        bool present;
        const char* value;
    };

    struct arg_spec
    {
        std::span<const option> options;
        std::string_view program_name;
        std::string_view version;
    };

    struct arg_data
    {
        std::vector<std::string> input_values;
        std::unordered_map<std::string, argument_value> values;
    };

    void make_help_msg(const arg_spec& options);
    auto parse_args(std::span<const char*> args, const arg_spec& options) -> arg_data;
} // namespace lexergen
