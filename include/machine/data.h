#pragma once

#include <cstdint>
#include <unordered_set>

namespace lexergen
{
    // TODO: replace this with a better data structure
    using state_set = std::unordered_set<std::int64_t>;
} // namespace lexergen
