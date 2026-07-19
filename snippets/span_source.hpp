#include <cstddef>
#include <cstdint>
#include <string_view>

class span_source
{
    const char* cur;
    const char* la;
    const char* end;
    const char* tok_start = nullptr;

    std::size_t bytes_ = 0;
    std::size_t la_bytes_ = 0;

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

        return static_cast<std::uint8_t>(ch);
    }

    void accept()
    {
        cur = la;
        bytes_ = la_bytes_;
    }

    void backtrack()
    {
        la = cur;
        la_bytes_ = bytes_;
    }

    void start_token() { tok_start = cur; }

    [[nodiscard]] auto text() const -> std::string_view { return {tok_start, cur}; }

    [[nodiscard]] auto bytes() const -> std::size_t { return bytes_; }

    [[nodiscard]] auto remaining() const -> std::string_view { return {la, static_cast<std::size_t>(end - la)}; }

    void skip(std::size_t n)
    {
        la += n;
        la_bytes_ += n;
    }
};
