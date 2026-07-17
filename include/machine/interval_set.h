#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

namespace lexergen
{
    class interval_set
    {
    public:
        using codepoint = std::uint32_t;

        struct interval
        {
            codepoint lo, hi;
            constexpr auto operator==(const interval&) const -> bool = default;
        };

    private:
        std::vector<interval> intervals;

        void normalize()
        {
            if (intervals.empty())
            {
                return;
            }

            std::ranges::sort(intervals, [](const interval& lhs, const interval& rhs) -> bool { return lhs.lo < rhs.lo; });

            std::vector<interval> merged;
            merged.push_back(intervals[0]);
            for (std::size_t i = 1; i < intervals.size(); i++)
            {
                auto& back = merged.back();
                if (intervals[i].lo <= back.hi + 1)
                {
                    back.hi = std::max(back.hi, intervals[i].hi);
                }
                else
                {
                    merged.push_back(intervals[i]);
                }
            }

            intervals = std::move(merged);
        }

    public:
        interval_set() = default;

        static auto single(codepoint cp) -> interval_set { return range(cp, cp); }

        static auto range(codepoint lo, codepoint hi) -> interval_set
        {
            interval_set set;
            set.intervals.push_back({.lo = lo, .hi = hi});
            return set;
        }

        [[nodiscard]] auto empty() const -> bool { return intervals.empty(); }
        [[nodiscard]] auto get_intervals() const -> const std::vector<interval>& { return intervals; }

        [[nodiscard]] auto max_codepoint() const -> codepoint
        {
            codepoint max_cp = 0;
            for (const auto& interval : intervals)
            {
                max_cp = std::max(max_cp, interval.hi);
            }
            return max_cp;
        }

        [[nodiscard]] auto contains(codepoint cp) const -> bool
        {
            auto it = std::upper_bound(intervals.begin(), intervals.end(), cp, [](codepoint c, const interval& iv) { return c < iv.lo; });
            return it != intervals.begin() && std::prev(it)->hi >= cp;
        }

        auto operator|=(const interval_set& rhs) -> interval_set&
        {
            intervals.insert(intervals.end(), rhs.intervals.begin(), rhs.intervals.end());
            normalize();
            return *this;
        }

        friend auto operator|(interval_set lhs, const interval_set& rhs) -> interval_set
        {
            lhs |= rhs;
            return lhs;
        }

        [[nodiscard]] auto operator~() const -> interval_set
        {
            constexpr codepoint BYTE_MAX_CP = 0xFF;

            for (const auto& interval : intervals)
            {
                if (interval.hi > BYTE_MAX_CP)
                {
                    throw std::runtime_error("cannot negate a charset containing a codepoint above 0xFF");
                }
            }

            interval_set result;
            codepoint next = 0;
            for (const auto& interval : intervals)
            {
                if (interval.lo > next)
                {
                    result.intervals.push_back({.lo = next, .hi = interval.lo - 1});
                }
                next = interval.hi + 1;
            }
            if (next <= BYTE_MAX_CP)
            {
                result.intervals.push_back({.lo = next, .hi = BYTE_MAX_CP});
            }
            return result;
        }
    };
} // namespace lexergen
