#include "regex.h"
#include <bitset>
#include <cassert>
#include <cctype>
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

        enum : uint8_t
        {
            TOK_CHAR,                   // regular char
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
        int index{};
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
                    return {.ch = (char) val, .type = tok::TOK_CHAR};
                }
                case 'x': { // Handle hexadecimal escape sequence
                    int val = 0;
                    for (int i = 0; i < 2 && index < str.size() && isxdigit(str[index]); i++)
                    {
                        char digit = str[index++];
                        val = (val << 4) | (isdigit(digit) ? digit - '0' : tolower(digit) - 'a' + 10);
                    }
                    return {.ch = (char) val, .type = tok::TOK_CHAR};
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
        regex_reader(const std::string& str) : str(str) { next(); }

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
                    new_chars = is_range ? character_range(token.ch, reader.curr().ch) : character(token.ch) | character('-');
                    break;
                });
                _case(tok::TOK_CHARSET_ALPHA, new_chars = alphanumeric());
                _case(tok::TOK_CHARSET_NOT_ALPHA, new_chars = ~alphanumeric());
                _case(tok::TOK_CHARSET_DIGIT, new_chars = digit());
                _case(tok::TOK_CHARSET_NOT_DIGIT, new_chars = ~digit());
                _case(tok::TOK_CHARSET_WHITESPACE, new_chars = whitespace());
                _case(tok::TOK_CHARSET_NOT_WHITESPACE, new_chars = ~whitespace());
            default:
                new_chars = character(token.ch);
            }

            current_charset = current_charset | new_chars;
        }

        return char_regex(negate ? ~current_charset : current_charset);
    }

    auto parse_atom(regex_reader& reader) -> regex;

    auto parse_quantifier(regex_reader& reader) -> regex
    {
        regex atom = parse_atom(reader);
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

    auto parse_atom(regex_reader& reader) -> regex
    {
        auto token = reader.next();
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
        default:
            return char_regex(token.ch);
        }
    }

    auto parse(regex_reader& reader)
    {
        // handle literal strings first
        if (reader.match(tok::TOK_STR))
        {
            reader.next();
            return parse_string(reader);
        }

        // handle actual regular expressions
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

auto lexergen::parse_regex(const std::string& str) -> regex_parse_result
{
    regex_reader reader(str);

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
