#include <cstddef>
#include <cstdint>
#include <deque>
#include <istream>
#include <string>
#include <string_view>

class stream_source
{
    std::istream& in;
    std::deque<char> buf;
    std::size_t peek_index = 0;
    std::string text_buf;

    std::size_t bytes_ = 0;
    std::size_t la_bytes_ = 0;

public:
    explicit stream_source(std::istream& in) : in(in) {}

    auto peek() -> std::uint8_t
    {
        if (peek_index < buf.size())
        {
            return static_cast<std::uint8_t>(buf[peek_index++]);
        }

        char ch = 0;
        in.get(ch);

        la_bytes_++;

        buf.push_back(ch);
        peek_index++;
        return static_cast<std::uint8_t>(ch);
    }

    void accept()
    {
        text_buf.insert(text_buf.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(peek_index));
        buf.erase(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(peek_index));
        peek_index = 0;
        bytes_ = la_bytes_;
    }

    void backtrack() { peek_index = 0; }

    void start_token() { text_buf.clear(); }

    [[nodiscard]] auto text() const -> std::string_view { return text_buf; }

    [[nodiscard]] auto bytes() const -> std::size_t { return bytes_; }
};
