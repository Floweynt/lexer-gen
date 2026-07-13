#include <cstddef>
#include <cstdint>
#include <string_view>

class span_source
{
    const char* cur;
    const char* la;
    const char* end;
    const char* tok_start = nullptr;

    std::size_t bytes_ = 0, col_ = 1, line_ = 1;
    std::size_t la_bytes_ = 0, la_col_ = 1, la_line_ = 1;

public:
    span_source(const char* begin, const char* end) : cur(begin), la(begin), end(end) {}

    auto peek() -> std::uint8_t
    {
        if (la >= end)
        {
            return 0;
        }

        char ch = *la++;
        la_bytes_++;
        la_col_++;
        if (ch == '\n')
        {
            la_line_++;
            la_col_ = 1;
        }

        return static_cast<std::uint8_t>(ch);
    }

    void accept()
    {
        cur = la;
        bytes_ = la_bytes_;
        col_ = la_col_;
        line_ = la_line_;
    }

    void backtrack()
    {
        la = cur;
        la_bytes_ = bytes_;
        la_col_ = col_;
        la_line_ = line_;
    }

    void start_token() { tok_start = cur; }

    [[nodiscard]] auto text() const -> std::string_view { return {tok_start, cur}; }

    [[nodiscard]] auto bytes() const -> std::size_t { return bytes_; }
    [[nodiscard]] auto col() const -> std::size_t { return col_; }
    [[nodiscard]] auto line() const -> std::size_t { return line_; }
};
