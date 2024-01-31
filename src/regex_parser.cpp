#include <iostream>
#include <regex.h>

namespace lexergen
{
    void regex_parser::error(std::string msg) { errors.emplace_back(std::move(msg)); }

    auto regex_parser::peek() const -> char { return curr_pos < curr_regex.size() ? curr_regex[curr_pos] : '\0'; }

    auto regex_parser::eof() const -> bool { return curr_pos >= curr_regex.size(); }

    auto regex_parser::match(char ch) -> bool
    {
        if (peek() == ch)
        {
            ++curr_pos;
            return true;
        }
        return false;
    }

    auto regex_parser::parse_char() -> regex
    {
        if (peek() == '\\')
        {
            match('\\');
            char ch = peek();

            if (ch == 'n')
            {
                next();
                return char_regex('\n');
            }
            if (ch == '\\')
            {
                next();
                return char_regex('\\');
            }

            if (ch == 'w' || ch == 'd' || ch == 's' || ch == 'W' || ch == 'D' || ch == 'S')
            {
                match(ch);
                switch (ch)
                {
                case 'w':
                    return char_regex(alphanumeric());
                case 'W':
                    return char_regex(!alphanumeric());
                case 'd':
                    return char_regex(digit());
                case 'D':
                    return char_regex(!digit());
                case 's':
                    return char_regex(whitespace());
                case 'S':
                    return char_regex(!whitespace());
                }
            }
            else
            {
                match(ch);
                return char_regex(ch);
            }
        }
        else
        {
            char ch = peek();
            if (ch != '|' && ch != ')' && ch != '*' && ch != '+' && ch != '?')
            {
                ++curr_pos;
                return char_regex(ch);
            }
        }
        return nullptr;
    }

    auto regex_parser::parse_char_class(const std::string& str, bool negate) -> regex
    {
        auto current_charset = empty();
        for (size_t i = 0; i < str.size(); i++)
        {
            if (i + 1 < str.size() && str[i + 1] == '-')
            {
                if (i + 2 == str.size())
                {
                    current_charset = current_charset || character(str[i + 1]);
                    i++;
                    break;
                }

                current_charset = current_charset || character_range(str[i], str[i + 2]);
                i += 2;
            }
            else
            {
                current_charset = current_charset || character(str[i]);
            }
        }

        return char_regex(negate ? !current_charset : current_charset);
    }

    // Parse an atom (character or capture group)
    auto regex_parser::parse_atom() -> regex
    {
        if (match('['))
        {
            std::string chars;
            bool negate = false;
            if (match('^'))
            {
                negate = true;
            }
            while (!match(']'))
            {
                if (eof())
                {
                    error("unterminated character set");
                    return nullptr;
                }
                if (match('\\'))
                {
                    chars += parse_escape();
                }
                else
                {
                    chars += next();
                }
            }

            return parse_char_class(chars, negate);
        }
        else if (match('('))
        {
            auto expr = parse_alt();
            if (match(')'))
            {
                return expr;
            }
            return nullptr;
        }
        else if (match('.'))
        {
            return char_regex(!empty());
        }

        return parse_char();
    }

    auto regex_parser::next() -> char
    {
        if (!eof())
        {
            return curr_regex[curr_pos++];
        }

        return '\0';
    }

    auto regex_parser::parse_escape() -> std::string
    {
        char ch = peek();

        if (ch == 'n')
        {
            next();
            return "\n";
        }

        if (ch == '\\')
        {
            next();
            return "\\";
        }

        if (ch == 'x')
        {
            next();
            std::string hex;
            for (int i = 0; i < 2; ++i)
            {
                char digit = next();
                if (!isxdigit(digit))
                {
                    error("invalid hexadecimal escape");
                    return "";
                }
                hex += digit;
            }
            return hex;
        }

        if (isdigit(ch))
        {
            std::string oct;
            for (int i = 0; i < 3; ++i)
            {
                char digit = peek();
                if (!isdigit(digit))
                {
                    break;
                }
                oct += next();
            }
            return oct;
        }

        next();
        return std::string(1, ch);
    }

    // Parse a character class (\w, \d, \s, etc.)
    auto regex_parser::parse_class() -> regex
    {
        if (match('\\'))
        {
            char ch = peek();
            if (ch == 'w' || ch == 'd' || ch == 's' || ch == 'W' || ch == 'D' || ch == 'S')
            {
                match(ch);
                switch (ch)
                {
                case 'w':
                    return char_regex(alphanumeric());
                case 'W':
                    return char_regex(!alphanumeric());
                case 'd':
                    return char_regex(digit());
                case 'D':
                    return char_regex(!digit());
                case 's':
                    return char_regex(whitespace());
                case 'S':
                    return char_regex(!whitespace());
                }
            }
        }
        return nullptr;
    }

    auto regex_parser::parse_capture() -> regex
    {
        if (match('('))
        {
            regex expr = parse_alt();
            if (match(')'))
            {
                return expr;
            }
            return nullptr;
        }
        return nullptr;
    }

    auto regex_parser::parse_quantifier() -> regex
    {
        regex atom = parse_atom();
        if (atom)
        {
            if (match('*'))
            {
                return star_regex(atom);
            }
            if (match('+'))
            {
                return plus_regex(atom);
            }
            if (match('?'))
            {
                return optional_regex(atom);
            }
            return atom;
        }
        return nullptr;
    }

    auto regex_parser::parse_concat() -> regex
    {
        regex left = parse_quantifier();
        if (left)
        {
            while (!eof())
            {
                regex right = parse_quantifier();
                if (right)
                {
                    left = left + right;
                }
                else
                {
                    break;
                }
            }
        }
        return left;
    }

    auto regex_parser::parse_alt() -> regex
    {
        regex left = parse_concat();
        if (left)
        {
            while (match('|'))
            {
                regex right = parse_concat();
                if (right)
                {
                    left = left | right;
                }
                else
                {
                    break;
                }
            }
        }
        return left;
    }

    auto regex_parser::parse_name() -> std::string
    {
        std::string name;
        while (!eof())
        {
            char ch = peek();
            if (isalnum(ch) || ch == '_')
            {
                name += ch;
                ++curr_pos;
            }
            else
            {
                break;
            }
        }
        return name;
    }

    auto regex_parser::parse() -> regex
    {
        regex root = parse_alt();
        if (!eof())
        {
            return nullptr;
        }
        return root;
    }

    void regex_parser::dump_error()
    {
        for (const auto& error : errors)
        {
            std::cerr << error << '\n';
        }
    }
} // namespace lexergen
