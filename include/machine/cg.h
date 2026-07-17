#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace lexergen
{
    enum class target_lang : std::uint8_t
    {
        CPP,
        C,
        JAVA,
        JS,
    };

    constexpr auto parse_target_lang(std::string_view name) -> std::optional<target_lang>
    {
        if (name == "cpp" || name == "c++")
        {
            return target_lang::CPP;
        }
        if (name == "c")
        {
            return target_lang::C;
        }
        if (name == "java")
        {
            return target_lang::JAVA;
        }
        if (name == "javascript" || name == "js")
        {
            return target_lang::JS;
        }
        return std::nullopt;
    }

    constexpr auto infer_target_lang(std::string_view filename) -> std::optional<target_lang>
    {
        auto dot = filename.find_last_of('.');
        if (dot == std::string_view::npos)
        {
            return std::nullopt;
        }

        auto ext = filename.substr(dot + 1);
        if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "cppm" || ext == "hpp" || ext == "hh" || ext == "hxx")
        {
            return target_lang::CPP;
        }
        if (ext == "c" || ext == "h")
        {
            return target_lang::C;
        }
        if (ext == "java")
        {
            return target_lang::JAVA;
        }
        if (ext == "js" || ext == "mjs" || ext == "cjs")
        {
            return target_lang::JS;
        }
        return std::nullopt;
    }

    constexpr auto base_fn_name(target_lang lang) -> std::string_view
    {
        switch (lang)
        {
        case target_lang::JAVA:
        case target_lang::JS:
            return "lexTok";
        case target_lang::CPP:
        case target_lang::C:
            break;
        }
        return "lex_tok";
    }
} // namespace lexergen
