# Lexer gen

Simple modern lexer generator.

## Example 

To start, you need to create a file that describes the lexer, which is broken up in multiple sections:
```leg
// Will be pasted at the top of the generated lexer; do includes here
#include <string>
#include <variant>
#include <format>
#include <iostream>
#include "lexer.h"
#define _curr start_bytes, src.bytes()
%%
# Lexer rules
# Use hashtags as comments in this context

# UNKNOWN fires when no rule matches; ERROR fires on an internal lexer bug.
# Like every rule, each is a single line.
UNKNOWN ctx.report_error(src.bytes(), "unknown token: failed to parse token"); throw lexer_error();
ERROR throw std::runtime_error("internal lexer error");

# Match comment, whitespace, EOF 
/\/\/[^\n]+/ break;
/\s+/ break;
"\0" return token(0, 0, token::TOK_EOF);

# identifiers 
/[a-zA-Z_]\w*/ return from_identifier(ctx, _curr, buffer);

# match integers
/\d+/ return integer_to_token(_curr, buffer);

# match operators
"+" return token(_curr, token::TOK_OPERATOR, token::OP_PLUS);
"-" return token(_curr, token::TOK_OPERATOR, token::OP_MINUS);
"*" return token(_curr, token::TOK_OPERATOR, token::OP_STAR);
"/" return token(_curr, token::TOK_OPERATOR, token::OP_DIV);

# punctuation
"(" return token(_curr, token::TOK_PAREN_OPEN);
")" return token(_curr, token::TOK_PAREN_CLOSE);
```

In this example, `token` and `lexer_error` are defined in `lexer.h`, along with any
application state you want your rule actions to see.

The generator emits:
```cpp
template <typename Source, typename Ctx>
auto lex_tok(Source& src, Ctx& ctx);
```

`Source` supplies raw bytes and tracks position/backtracking; `Ctx` is your own state,
passed through untouched. Generated output is dependency-free; copy a `Source` into
your preamble:
- `snippets/stream_source.hpp` - for `std::istream`
- `snippets/span_source.hpp` - pointer-pair over an in-memory buffer, zero-copy

Or write your own. A `Source` must provide:
| method | purpose |
|---|---|
| `uint8_t peek()` | consume and return the next lookahead byte, `0` forever at EOF |
| `void accept()` | commit everything scanned so far as part of the current token |
| `void backtrack()` | rewind lookahead back to the last `accept()` |
| `void start_token()` | called when a new token starts scanning |
| `text() const` | bytes from `start_token()` to the last `accept()` (string-view-like) |
| `bytes() const` | byte offset as of the last `accept()` |

Make sure that all rules (EOF and user defined) return the same type. `Source` only
tracks byte offsets; line/column tracking is on you if you want it, e.g. counting `\n`
(or `\r`/`\r\n`) in `text()` inside the rules that can contain them.

To generate source, run:

```bash 
lexer-gen lexer.leg -o lexer_impl.cpp
```

More examples can be found under the `examples` directory.

## Macros

`MACRO name /expr/` defines a reusable regex fragment, referenced from any later pattern
(including other macros) as `{name}`:
```leg
MACRO digit /[0-9]/
MACRO ident_start /[a-zA-Z_]/

/{ident_start}({ident_start}|{digit})*/ return from_identifier(ctx, _curr, buffer);
/{digit}+(\.{digit}+)?/ return integer_to_token(_curr, buffer);
```
`{name}` is only recognized as a macro reference inside a bare `/regex/`, not inside
`"quoted strings"` or `[char classes]`, so `"{"` still matches a literal `{` byte.

## Stateful lexing

`STATE name { ... }` scopes a block of rules (its own `RULE`/`MACRO` lines) into their
own DFA, compiled and emitted as their own entry point, e.g. `lex_tok_string(src, ctx)`
next to the default `lex_tok(src, ctx)`. lexer-gen does not track "the current state"
itself: which entry point to call is entirely up to your own driver code, e.g.:
```leg
UNKNOWN ...
ERROR ...

"\"" ctx.push(IN_STRING); return token{token::TOK_QUOTE};
/{XID_Start}{XID_Continue}*/ return from_identifier(ctx, _curr, buffer);

STATE string {
    "\"" ctx.pop(); return token{token::TOK_QUOTE};
    /[^"\\]+/ return token{token::TOK_STRING_CHUNK, std::string(src.text())};
}
```
```cpp
token lex(Source& src, Ctx& ctx) {
    return ctx.top() == IN_STRING ? lex_tok_string(src, ctx) : lex_tok(src, ctx);
}
```
A rule's action only hands control back to the caller when it `return`s; a rule ending
in `break` (skip this token, e.g. whitespace) keeps scanning within the *same* DFA it's
already in, so a state change made from a `break`-only rule won't take effect until that
call eventually does `return` something. If you want a state switch to apply to the very
next token, `return` immediately after making it (as above) rather than `break`ing.

## Target languages

`-l`/`--lang` picks the target language: `cpp` (default), `c`, `java`, `javascript`. If
omitted, it's inferred from `-o`'s extension. Each target's rule actions must be written in
that language, and the `Source` snippet you copy in must match it:

| target | dispatch strategy | `Source` snippet |
|---|---|---|
| cpp | `goto`-threaded, `Source`/`Ctx` are template params | `snippets/stream_source.hpp`, `snippets/span_source.hpp` |
| c | `goto`-threaded, `Source`/`Ctx` are concrete types you typedef; methods are free functions `Source_peek(src)` etc. | `snippets/span_source.h` |
| java | unthreaded switch; `Source` must be a concrete class named exactly `Source` | `snippets/Source.java` |
| javascript | same switch-loop strategy as java, but untyped | `snippets/span_source.js` |

Function name and calling convention follow each language's idiom: `lex_tok(src, ctx)`
in cpp/c, `lexTok(src, ctx)` in java/javascript.

## Building 
Make sure you have `meson` installed, as well as a `c++20` (or later) compatible compiler with `<format>` support.

Run:
```bash
$ meson setup build --buildtype release
$ cd release
$ ninja or make -j12 
```

## Debugging 

It is possible to dump the internal NFA (generated from the regular expressions) and the DFA (generated from NFA) as dot graphs:
- `-D` - dump DFA
- `-N` - dump NFA

## Unicode

Patterns can reference codepoints via `\u{XXXX}` (a single codepoint) or, inside a
`[...]` char class, `\u{XXXX}-\u{YYYY}` (an inclusive codepoint range), e.g.:
```leg
MACRO cjk_ident /[\u{4E00}-\u{9FFF}]+/
```

Two macros are built into every grammar's macro table, so no `MACRO` line is needed to
use them (a grammar can still redefine either name to override it): `{XID_Start}` and
`{XID_Continue}`. Note `XID_Start` excludes `_`; add it explicitly
(`/(_|{XID_Start}){XID_Continue}*/`) for languages like Python that allow a leading
underscore. The tables are generated from Python's `unicodedata`; regenerate via
`tools/gen_unicode_identifier_ranges.py` to bump the Unicode version.

## Further optimization ideas
- SIMD/SWAR scanning for common runs (whitespace, identifiers, digits) instead of one
  byte/codepoint at a time.
- Hot-state layout: order state labels/cases by actual traversal frequency so the
  common path is contiguous.

## TODO
- Parser generator

