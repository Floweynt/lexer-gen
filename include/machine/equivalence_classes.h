#pragma once

#include "machine/interval_set.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

namespace lexergen
{
    class equivalence_classes
    {
        std::vector<interval_set::codepoint> boundaries;

    public:
        static auto build(const std::vector<interval_set>& charsets) -> equivalence_classes
        {
            std::vector<interval_set::codepoint> points;

            for (const auto& set : charsets)
            {
                for (const auto& iv : set.get_intervals())
                {
                    points.push_back(iv.lo);
                    if (iv.hi < UINT32_MAX)
                    {
                        points.push_back(iv.hi + 1);
                    }
                }
            }

            std::ranges::sort(points);
            points.erase(std::unique(points.begin(), points.end()), points.end());

            equivalence_classes result;
            result.boundaries = std::move(points);
            return result;
        }

        [[nodiscard]] auto class_count() const -> std::size_t { return boundaries.empty() ? 0 : boundaries.size() - 1; }

        [[nodiscard]] auto classify(interval_set::codepoint codepoint) const -> std::int64_t
        {
            auto iter = std::ranges::upper_bound(boundaries, codepoint);
            if (iter == boundaries.begin())
            {
                return static_cast<std::int64_t>(class_count());
            }

            auto idx = static_cast<std::int64_t>(std::distance(boundaries.begin(), iter)) - 1;
            if (idx >= static_cast<std::int64_t>(class_count()))
            {
                return static_cast<std::int64_t>(class_count());
            }

            return idx;
        }

        [[nodiscard]] auto class_range_for(interval_set::codepoint lo, interval_set::codepoint hi) const -> std::pair<std::int64_t, std::int64_t>
        { return {classify(lo), classify(hi)}; }

        [[nodiscard]] auto class_interval(std::int64_t class_id) const -> interval_set::interval
        {
            return {
                .lo = boundaries[static_cast<std::size_t>(class_id)],
                .hi = boundaries[static_cast<std::size_t>(class_id) + 1] - 1,
            };
        }

        [[nodiscard]] auto max_codepoint() const -> interval_set::codepoint { return boundaries.empty() ? 0 : boundaries.back() - 1; }

        [[nodiscard]] auto get_boundaries() const -> const std::vector<interval_set::codepoint>& { return boundaries; }
    };
} // namespace lexergen
