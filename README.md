# Lexer gen

Simple modern `c++14` (or later) lexer generator. 

## Example 

To start, you need to create a file that describes the lexer, which is broken up in multiple sections:
```leg
// Will be pasted at the top of the generated lexer; do includes here
#include <string>
#include <variant>
#include <fmt/core.h>
#include "lexer.h"
#define _curr {start_line, start_col, start_bytes}, {ctx.line, ctx.col, ctx.bytes}
%%
// Token lex fail reporting
ctx.ctx.report_error({ctx.line, ctx.col, ctx.bytes}, "unknown token: failed to parse token"); 
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
/[a-zA-Z_]\w*/ return from_identifier(ctx.ctx, _curr, buffer);

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

In this example, a type called `token` is defined in `lexer.h`. 
In `lexer.h`, you must define a type:
```cpp
struct lex_context
{
    std::istream& in;
    std::deque<char> buffer;
    size_t peek_index;

    size_t bytes, col, line;
    size_t pbytes, pcol, pline;
};
```

In turn, the lexer generator will create a function:
```cpp
auto lex_tok(lex_context& ctx);
```

Make sure that all rules (EOF and user defined) return the same type.

To generate source, run: 

```bash 
lexer-gen lexer.leg -o lexer_impl.cpp
```

Optionally add the `-c` flag to enable equivalence classes.

More examples can be found under the `examples` directory.

## Building 
Make sure you have `fmt` and `meson` installed, as well as a `c++20` compatible compiler.

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

## Advanced usage 



## TODO 
- Support for other languages like `c`, `js`, `java`, etc
- Better regular expression parsing (implement macros to reduce code duplication, with syntax like `{macro_name}`)
- Better lexer specification parsing in general
- Compact tables by using smaller bit-width types whenever possible 
- Table padding and alignment
- Actually release the tree-sitter grammar
- Parser generator

