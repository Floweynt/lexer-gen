#include "diagnostics.h"
#include <cstdlib>
#include <unistd.h>

namespace
{
    auto supports_color() -> bool { return std::getenv("NO_COLOR") == nullptr && isatty(STDERR_FILENO) != 0; }
} // namespace

auto lexergen::warn_prefix() -> const std::string&
{
    static const std::string prefix = supports_color() ? "\033[1;33mwarning:\033[0m " : "warning: ";
    return prefix;
}
