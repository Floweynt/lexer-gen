# Lexer gen

Simple modern `c++20` (or later) lexer generator. 

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

## TODO 
- Unicode support
- Other target languages (`c`, `js`, `java`, etc.)
- Better regular expression parsing (macros, e.g. `{macro_name}`)
- Better lexer specification parsing in general
- Actually release the tree-sitter grammar
- Parser generator

