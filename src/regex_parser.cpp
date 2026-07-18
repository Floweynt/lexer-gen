#include "regex.h"
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#define YIELD_TOK(sw, t)                                                                                                                             \
    case sw:                                                                                                                                         \
        return { .ch = ch, .type = t }
#define YIELD_CH(sw, c)                                                                                                                              \
    case sw:                                                                                                                                         \
        return { .ch = c, .type = tok::TOK_CHAR }

#define _case(cond, body)                                                                                                                            \
    case cond: {                                                                                                                                     \
        body;                                                                                                                                        \
    }                                                                                                                                                \
    break

using namespace lexergen;

namespace
{
    struct tok
    {
        char ch;
        uint32_t codepoint = 0;

        enum : uint8_t
        {
            TOK_CHAR,                   // regular char
            TOK_CODEPOINT,              // \u{XXXX}
            TOK_GROUP_OPEN,             // (
            TOK_GROUP_CLOSE,            // )
            TOK_CHAR_OPEN,              // [
            TOK_CHAR_CLOSE,             // ]
            TOK_PLUS,                   // +
            TOK_WILDCARD,               // .
            TOK_STAR,                   // *
            TOK_OPT,                    // ?
            TOK_ALT,                    // |
            TOK_STR,                    // "
            TOK_NOT,                    // ^
            TOK_DASH,                   // -
            TOK_CHARSET_ALPHA,          // \w
            TOK_CHARSET_NOT_ALPHA,      // \W
            TOK_CHARSET_DIGIT,          // \d
            TOK_CHARSET_NOT_DIGIT,      // \D
            TOK_CHARSET_WHITESPACE,     // \s
            TOK_CHARSET_NOT_WHITESPACE, // \S
            TOK_DELIM,                  // /
            TOK_EOF,
        } type;
    };

    struct regex_parse_error : std::runtime_error
    {
        regex_parse_error(const std::string& str) : std::runtime_error(str) {}
    };

    struct regex_reader
    {
        const std::string& str;
        const macro_table& macros;
        size_t index{};
        tok curr_token{};

        auto lex_tok() -> tok
        {

            if (index >= str.size())
            {
                return {.ch = '\0', .type = tok::TOK_EOF};
            }

            char ch = str[index++];

            switch (ch)
            {
            case '\\': {
                if (index == str.size())
                {
                    return {.ch = ch, .type = tok::TOK_CHAR};
                }

                ch = str[index++];

                switch (ch)
                {
                    YIELD_TOK('w', tok::TOK_CHARSET_ALPHA);
                    YIELD_TOK('W', tok::TOK_CHARSET_NOT_ALPHA);
                    YIELD_TOK('d', tok::TOK_CHARSET_DIGIT);
                    YIELD_TOK('D', tok::TOK_CHARSET_NOT_DIGIT);
                    YIELD_TOK('s', tok::TOK_CHARSET_WHITESPACE);
                    YIELD_TOK('S', tok::TOK_CHARSET_NOT_WHITESPACE);
                    YIELD_CH('a', '\a');
                    YIELD_CH('f', '\f');
                    YIELD_CH('n', '\n');
                    YIELD_CH('r', '\r');
                    YIELD_CH('t', '\t');
                    YIELD_CH('v', '\v');
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7': { // Handle octal escape sequence
                    int val = ch - '0';
                    // eat the *next* 2 characters
                    for (int i = 0; i < 2 && index < str.size() && str[index] >= '0' && str[index] <= '7'; i++)
                    {
                        val = (val << 3) | (str[index++] - '0');
                    }
                    return {.ch = (char)val, .type = tok::TOK_CHAR};
                }
                case 'x': { // Handle hexadecimal escape sequence
                    int val = 0;
                    for (int i = 0; i < 2 && index < str.size() && isxdigit(str[index]); i++)
                    {
                        char digit = str[index++];
                        val = (val << 4) | (isdigit(digit) ? digit - '0' : tolower(digit) - 'a' + 10);
                    }
                    return {.ch = (char)val, .type = tok::TOK_CHAR};
                }
                case 'u': { // Handle \u{XXXX} unicode codepoint escape
                    if (index >= str.size() || str[index] != '{')
                    {
                        throw regex_parse_error("expected '{' after \\u");
                    }
                    index++;

                    uint32_t val = 0;
                    bool any_digit = false;
                    while (index < str.size() && isxdigit(str[index]))
                    {
                        char digit = str[index++];
                        val = (val << 4) | static_cast<uint32_t>(isdigit(digit) ? digit - '0' : tolower(digit) - 'a' + 10);
                        any_digit = true;
                    }

                    if (!any_digit || index >= str.size() || str[index] != '}')
                    {
                        throw regex_parse_error("malformed \\u{...} escape, expected \\u{HEX}");
                    }
                    index++;

                    return {.ch = 0, .codepoint = val, .type = tok::TOK_CODEPOINT};
                }
                default:
                    return {.ch = ch, .type = tok::TOK_CHAR};
                }
            }
                YIELD_TOK('(', tok::TOK_GROUP_OPEN);
                YIELD_TOK(')', tok::TOK_GROUP_CLOSE);
                YIELD_TOK('[', tok::TOK_CHAR_OPEN);
                YIELD_TOK(']', tok::TOK_CHAR_CLOSE);
                YIELD_TOK('+', tok::TOK_PLUS);
                YIELD_TOK('.', tok::TOK_WILDCARD);
                YIELD_TOK('*', tok::TOK_STAR);
                YIELD_TOK('?', tok::TOK_OPT);
                YIELD_TOK('|', tok::TOK_ALT);
                YIELD_TOK('"', tok::TOK_STR);
                YIELD_TOK('^', tok::TOK_NOT);
                YIELD_TOK('-', tok::TOK_DASH);
                YIELD_TOK('/', tok::TOK_DELIM);
            default:
                return {.ch = ch, .type = tok::TOK_CHAR};
            }
        }

    public:
        regex_reader(const std::string& str, const macro_table& macros) : str(str), macros(macros) { next(); }

        auto lookup_macro(const std::string& name) -> regex
        {
            auto iter = macros.find(name);
            if (iter == macros.end())
            {
                throw regex_parse_error("undefined macro '{" + name + "}'");
            }
            return iter->second;
        }

        auto next() -> tok
        {
            if (curr_token.type == tok::TOK_EOF)
            {
                throw regex_parse_error("unexpected end of regex");
            }

            auto res = curr_token;
            curr_token = lex_tok();
            return res;
        }

        [[nodiscard]] auto curr() const -> tok
        {
            if (curr_token.type == tok::TOK_EOF)
            {
                throw regex_parse_error("unexpected end of regex");
            }

            return curr_token;
        }

        [[nodiscard]] auto has() const -> bool { return curr_token.type != tok::TOK_EOF; }

        template <typename... Args>
        auto match(Args... args)
        {
            return (... || (args == curr().type));
        }

        template <typename... Args>
        auto match_advance(Args... args)
        {
            return (... || (args == next().type));
        }

        auto get_remaining() -> std::string
        {
            if (index >= str.size())
            {
                return "";
            }
            return str.substr(index);
        }
    };

    auto tok_codepoint(const tok& token) -> char_set::codepoint
    {
        return token.type == tok::TOK_CODEPOINT ? token.codepoint : static_cast<uint8_t>(token.ch);
    }

    auto tok_charset(const tok& token) -> char_set { return char_set::single(tok_codepoint(token)); }

    auto parse_char_class(regex_reader& reader) -> regex
    {
        tok token{};
        bool negate = false;
        auto current_charset = empty();

        if (reader.curr().type == tok::TOK_NOT)
        {
            reader.next();
            negate = true;
        }

        while ((token = reader.next()).type != tok::TOK_CHAR_CLOSE)
        {
            // token = current token
            // reader.curr() = next token
            char_set new_chars;

            switch (reader.curr().type)
            {
                _case(tok::TOK_DASH, {
                    reader.next();
                    auto is_range = reader.curr().type != tok::TOK_CHAR_CLOSE;
                    new_chars = is_range ? codepoint_range(tok_codepoint(token), tok_codepoint(reader.curr())) : tok_charset(token) | character('-');
                    break;
                });
                _case(tok::TOK_CHARSET_ALPHA, new_chars = alphanumeric());
                _case(tok::TOK_CHARSET_NOT_ALPHA, new_chars = ~alphanumeric());
                _case(tok::TOK_CHARSET_DIGIT, new_chars = digit());
                _case(tok::TOK_CHARSET_NOT_DIGIT, new_chars = ~digit());
                _case(tok::TOK_CHARSET_WHITESPACE, new_chars = whitespace());
                _case(tok::TOK_CHARSET_NOT_WHITESPACE, new_chars = ~whitespace());
            default:
                new_chars = tok_charset(token);
            }

            current_charset = current_charset | new_chars;
        }

        return char_regex(negate ? ~current_charset : current_charset);
    }

    auto parse_atom(regex_reader& reader) -> regex;

    constexpr int64_t UNBOUNDED = -1;

    auto parse_bound_number(regex_reader& reader) -> int64_t
    {
        std::string digits;
        while (reader.curr().type == tok::TOK_CHAR && isdigit(static_cast<unsigned char>(reader.curr().ch)))
        {
            digits += reader.next().ch;
        }

        if (digits.empty())
        {
            throw regex_parse_error("expected digit in bound repeat");
        }

        return std::stoll(digits);
    }

    auto parse_repeat_bound(regex_reader& reader) -> std::pair<int64_t, int64_t>
    {
        int64_t min_count = parse_bound_number(reader);
        int64_t max_count = min_count;

        if (reader.curr().type == tok::TOK_CHAR && reader.curr().ch == ',')
        {
            reader.next();
            max_count =
                reader.curr().type == tok::TOK_CHAR && isdigit(static_cast<unsigned char>(reader.curr().ch)) ? parse_bound_number(reader) : UNBOUNDED;
        }

        if (reader.curr().type != tok::TOK_CHAR || reader.curr().ch != '}')
        {
            throw regex_parse_error("expected '}' to close bound repeat");
        }
        reader.next();

        if (max_count != UNBOUNDED && max_count < min_count)
        {
            throw regex_parse_error("bound repeat max is less than min");
        }

        return {min_count, max_count};
    }

    auto repeat_regex(const regex& atom, int64_t min_count, int64_t max_count) -> regex
    {
        if (min_count == 0 && max_count == 0)
        {
            throw regex_parse_error("bound repeat {0} matches nothing, which is not supported");
        }

        regex result;
        for (int64_t i = 0; i < min_count; i++)
        {
            result = result ? result + atom : atom;
        }

        if (max_count == UNBOUNDED)
        {
            result = result ? result + star_regex(atom) : star_regex(atom);
        }
        else
        {
            for (int64_t i = min_count; i < max_count; i++)
            {
                result = result ? result + optional_regex(atom) : optional_regex(atom);
            }
        }

        return result;
    }

    auto parse_quantifier(regex_reader& reader) -> regex
    {
        regex atom = parse_atom(reader);

        if (reader.curr().type == tok::TOK_CHAR && reader.curr().ch == '{')
        {
            auto saved_index = reader.index;
            auto saved_curr = reader.curr_token;

            reader.next();
            if (reader.curr().type == tok::TOK_CHAR && isdigit(static_cast<unsigned char>(reader.curr().ch)))
            {
                auto [min_count, max_count] = parse_repeat_bound(reader);
                return repeat_regex(atom, min_count, max_count);
            }

            reader.index = saved_index;
            reader.curr_token = saved_curr;
        }

        regex (*wrapper)(const regex&) = nullptr;

        switch (reader.curr().type)
        {
            _case(tok::TOK_STAR, wrapper = star_regex);
            _case(tok::TOK_PLUS, wrapper = plus_regex);
            _case(tok::TOK_OPT, wrapper = optional_regex);
        default:
            break;
        }

        if (wrapper != nullptr)
        {
            reader.next();
            return wrapper(atom);
        }

        return atom;
    }

    auto parse_concat(regex_reader& reader)
    {
        auto base = parse_quantifier(reader);

        while (reader.has() && !reader.match(tok::TOK_ALT, tok::TOK_GROUP_CLOSE, tok::TOK_DELIM))
        {
            base = base + parse_quantifier(reader);
        }

        return base;
    }

    auto parse_alt(regex_reader& reader)
    {
        auto base = parse_concat(reader);

        while (reader.has() && reader.match(tok::TOK_ALT))
        {
            reader.next();
            base = base | parse_concat(reader);
        }

        return base;
    }

    auto parse_group(regex_reader& reader) -> regex
    {
        auto alt = parse_alt(reader);
        if (!reader.match_advance(tok::TOK_GROUP_CLOSE))
        {
            throw regex_parse_error("unclosed group");
        }

        return alt;
    }

    auto parse_string(regex_reader& reader)
    {
        std::string str;
        tok token{};
        while ((token = reader.next()).type != tok::TOK_STR)
        {
            str += token.ch;
        }

        return string_regex(std::move(str));
    }

    auto parse_macro_ref(regex_reader& reader) -> regex
    {
        std::string name;
        tok token{};
        while ((token = reader.next()).ch != '}')
        {
            if (token.type != tok::TOK_CHAR)
            {
                throw regex_parse_error("invalid character in macro reference");
            }
            name += token.ch;
        }

        return reader.lookup_macro(name);
    }

    auto parse_atom(regex_reader& reader) -> regex
    {
        auto token = reader.next();

        if (token.type == tok::TOK_CHAR && token.ch == '{')
        {
            return parse_macro_ref(reader);
        }

        switch (token.type)
        {
            _case(tok::TOK_CHAR_OPEN, return parse_char_class(reader));
            _case(tok::TOK_CHARSET_ALPHA, return char_regex(alphanumeric()));
            _case(tok::TOK_CHARSET_NOT_ALPHA, return char_regex(~alphanumeric()));
            _case(tok::TOK_CHARSET_DIGIT, return char_regex(digit()));
            _case(tok::TOK_CHARSET_NOT_DIGIT, return char_regex(~digit()));
            _case(tok::TOK_CHARSET_WHITESPACE, return char_regex(whitespace()));
            _case(tok::TOK_CHARSET_NOT_WHITESPACE, return char_regex(~whitespace()));
            _case(tok::TOK_GROUP_OPEN, return parse_group(reader));
            _case(tok::TOK_WILDCARD, return char_regex(~empty()));
            _case(tok::TOK_STR, return parse_string(reader));
            _case(tok::TOK_CODEPOINT, return char_regex(char_set::single(token.codepoint)));
        default:
            return char_regex(token.ch);
        }
    }

    auto parse(regex_reader& reader)
    {
        if (reader.match(tok::TOK_STR))
        {
            reader.next();
            return parse_string(reader);
        }

        if (!reader.match_advance(tok::TOK_DELIM))
        {
            throw regex_parse_error("expected / at the start of regex");
        }

        auto res = parse_alt(reader);

        if (!reader.match_advance(tok::TOK_DELIM))
        {
            throw regex_parse_error("expected / at the end of regex");
        }

        return res;
    }
} // namespace

auto lexergen::parse_regex(const std::string& str, const macro_table& macros) -> regex_parse_result
{
    regex_reader reader(str, macros);

    try
    {
        return {
            .is_success = true,
            .expr = parse(reader),
            .rest = reader.get_remaining(),
        };
    }
    catch (const regex_parse_error& parse_error)
    {
        return {
            .is_success = false,
            .error_msg = parse_error.what(),
        };
    }
}
