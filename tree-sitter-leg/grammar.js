module.exports = grammar({
    name: "leg",

    extras: ($) => [/[ \t]/],

    rules: {
        source_file: ($) => seq(
            field("preamble", optional($.preamble)),
            $.section_separator,
            repeat(choice($.macro_def, $.unknown_directive, $.error_directive, $.rule, $.comment, $.blank_line)),
            $.section_separator,
            field("epilogue", optional($.epilogue)),
        ),

        preamble: ($) => repeat1($._raw_line),
        epilogue: ($) => repeat1($._raw_line),

        section_separator: ($) => token(prec(10, seq(/[ \t]*/, "%%", /[ \t]*/, optional(/\r?\n/)))),

        _raw_line: ($) => token(prec(1, /[^\n]*\n/)),

        blank_line: ($) => token(/[ \t]*\r?\n/),
        comment: ($) => token(seq(/[ \t]*/, choice("#", ";"), /[^\n]*/, /\r?\n/)),

        _eol: ($) => token(/\r?\n/),

        macro_def: ($) => seq("MACRO", field("name", $.identifier), field("pattern", $._pattern), optional($._action), $._eol),

        unknown_directive: ($) => seq("UNKNOWN", optional($._action), $._eol),
        error_directive: ($) => seq("ERROR", optional($._action), $._eol),
        rule: ($) => seq(optional("RULE"), field("pattern", $._pattern), field("action", optional($._action)), $._eol),

        identifier: ($) => /[A-Za-z_][A-Za-z0-9_]*/,

        _pattern: ($) => choice($.regex, $.string_literal),
        _action: ($) => token(prec(1, /[^\n]+/)),

        regex: ($) => seq("/", repeat($._regex_term), "/"),

        _regex_term: ($) => choice(
            $.char_class,
            $.macro_ref,
            $.escape,
            $.codepoint_escape,
            $.group,
            $.quantifier,
            $.alternation_bar,
            $.wildcard,
            $.literal_char,
        ),

        group: ($) => seq("(", repeat($._regex_term), ")"),
        quantifier: ($) => choice("*", "+", "?"),
        alternation_bar: ($) => "|",
        wildcard: ($) => ".",

        char_class: ($) => seq("[", optional("^"), repeat($._char_class_item), "]"),
        _char_class_item: ($) => choice($.char_range, $.macro_ref, $.escape, $.codepoint_escape, $.literal_char),
        char_range: ($) => seq(choice($.escape, $.codepoint_escape, $.literal_char), "-", choice($.escape, $.codepoint_escape, $.literal_char)),
        macro_ref: ($) => seq("{", field("name", $.identifier), "}"),

        escape: ($) => token(seq("\\", choice(/[wWdDsSafnrtv\\\/\.\*\+\?\|\(\)\[\]\^\-"]/, /[0-7]{1,3}/, /x[0-9a-fA-F]{1,2}/))),

        codepoint_escape: ($) => token(seq("\\u{", /[0-9a-fA-F]+/, "}")),

        literal_char: ($) => /[^\n]/,

        string_literal: ($) => seq('"', repeat(choice($.escape, $.codepoint_escape, /[^"\n\\]/)), '"'),
    },
});
