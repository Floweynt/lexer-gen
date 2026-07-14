[
  "MACRO"
  "UNKNOWN"
  "ERROR"
  "RULE"
] @keyword

(section_separator) @punctuation.special

(comment) @comment

(macro_def name: (identifier) @variable.parameter)
(macro_ref name: (identifier) @variable.parameter)

(string_literal) @string
(regex) @string.regex

(escape) @string.escape
(codepoint_escape) @string.escape

[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @punctuation.bracket

[
  "/"
] @punctuation.delimiter

[
  "*"
  "+"
  "?"
] @operator

(alternation_bar) @operator
(wildcard) @constant.builtin
(char_range "-" @operator)
"^" @operator

(literal_char) @string
