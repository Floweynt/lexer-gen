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
#define _curr {start_line, start_col, start_bytes}, {src.line(), src.col(), src.bytes()}
%%
// Token lex fail reporting
ctx.report_error({src.line(), src.col(), src.bytes()}, "unknown token: failed to parse token");
throw lexer_error();
%%
throw std::runtime_error("internal lexer error");
%%
# Lexer rules
# Use hashtags as comments in this context

# Match comment, whitespace, EOF 
/\/\/[^\n]+/ break;
/\s+/ break;
"\0" return token({0, 0, 0}, {0, 0, 0}, token::TOK_EOF);

# identifiers 
/[a-zA-Z_]\w*/ return from_identifier(ctx, _curr, buffer);

# match integers
\d+ return integer_to_token(_curr, buffer);

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
| `line() / col() / bytes() const` | position as of the last `accept()` |

Make sure that all rules (EOF and user defined) return the same type.

To generate source, run:

```bash 
lexer-gen lexer.leg -o lexer_impl.cpp
```

More examples can be found under the `examples` directory.

## Target languages

`-l`/`--lang` picks the target language: `cpp` (default), `c`, `java`, `javascript`, `python`. If
omitted, it's inferred from `-o`'s extension. Each target's rule actions must be written in
that language, and the `Source` snippet you copy in must match it:

| target | dispatch strategy | `Source` snippet |
|---|---|---|
| cpp | `goto`-threaded, `Source`/`Ctx` are template params | `snippets/stream_source.hpp`, `snippets/span_source.hpp` |
| c | `goto`-threaded, `Source`/`Ctx` are concrete types you typedef; methods are free functions `Source_peek(src)` etc. | `snippets/span_source.c.h` |
| java | unthreaded switch; `Source` must be a concrete class named exactly `Source` | `snippets/Source.java` |
| javascript | same switch-loop strategy as java, but untyped | `snippets/span_source.js` |
| python | LUT | `snippets/span_source.py` |

Function name and calling convention follow each language's idiom: `lex_tok(src, ctx)`
in cpp/c/python, `lexTok(src, ctx)` in java/javascript.

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

## Further optimization ideas
- Shared equivalence classes to compress switch tables: keep per-state `goto`/`continue`
  threading but classify bytes with one global classifier first, so each state's switch
  has fewer, denser case values (smaller generated code, better icache) without
  reintroducing a runtime transition array.
- SIMD/SWAR scanning for common runs (whitespace, identifiers, digits) instead of one
  byte at a time.
- Hot-state layout: order state labels/cases by actual traversal frequency so the
  common path is contiguous.

## TODO 
- Unicode support
- Better regular expression parsing (macros, e.g. `{macro_name}`)
- Better lexer specification parsing in general
- Actually release the tree-sitter grammar
- Parser generator

