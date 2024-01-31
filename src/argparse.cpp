#include <argparse.h>

namespace lexergen
{
    using namespace std::string_literals;

    void make_help_msg(const arg_spec& options)
    {
        std::cerr << "usage: " << options.program_name << " [files] [flags]" << '\n';
        for (const auto& opt : options.options)
        {
            std::cerr << fmt::format("    {: <10} {: <10}: {}\n", opt.short_flag, opt.long_flag, opt.description);
        }
    }

    auto parse_args(const std::span<const char*> args, const arg_spec& options) -> arg_data
    {
        std::unordered_map<std::string, argument_value> values;

        bool is_await_args = false;
        std::string await_args_name;
        std::vector<std::string> input_values;

        for (const auto* arg : args)
        {
            if (is_await_args)
            {
                values[await_args_name].value = arg;
                is_await_args = false;
                continue;
            }

            if (arg == "--help"s)
            {
                make_help_msg(options);
                exit(0);
            }

            if (arg == "--version"s)
            {
                std::cerr << options.program_name << ": " << options.version << '\n';
                exit(0);
            }

            if (!std::string_view(arg).starts_with("-"))
            {
                input_values.emplace_back(arg);
                continue;
            }

            bool found = false;
            for (const auto& opt : options.options)
            {
                if (arg == opt.long_flag || arg == opt.short_flag)
                {
                    values[await_args_name = opt.name].present = true;
                    is_await_args = opt.has_args;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                std::cerr << "unknown option: " << arg << '\n';
                exit(-1);
            }
        }

        for (const auto& opt : options.options)
        {
            if (!values[std::string(opt.name)].present && opt.required)
            {
                std::cerr << "missing required option " << opt.long_flag << '\n';
                exit(-1);
            }
        }

        return arg_data{input_values, values};
    }
} // namespace lexergen
