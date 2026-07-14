#include <stddef.h>
#include <stdint.h>

typedef struct
{
    const char* cur;
    const char* la;
    const char* end;
    const char* tok_start;

    size_t bytes, col, line;
    size_t la_bytes, la_col, la_line;
} Source;

static void Source_init(Source* s, const char* begin, const char* end)
{
    s->cur = begin;
    s->la = begin;
    s->end = end;
    s->tok_start = begin;
    s->bytes = 0;
    s->col = 1;
    s->line = 1;
    s->la_bytes = 0;
    s->la_col = 1;
    s->la_line = 1;
}

static int Source_peek(Source* s)
{
    if (s->la >= s->end)
    {
        return 0;
    }

    unsigned char ch = (unsigned char)*s->la++;
    s->la_bytes++;
    s->la_col++;
    if (ch == '\n')
    {
        s->la_line++;
        s->la_col = 1;
    }

    return ch;
}

static void Source_accept(Source* s)
{
    s->cur = s->la;
    s->bytes = s->la_bytes;
    s->col = s->la_col;
    s->line = s->la_line;
}

static void Source_backtrack(Source* s)
{
    s->la = s->cur;
    s->la_bytes = s->bytes;
    s->la_col = s->col;
    s->la_line = s->line;
}

static void Source_start_token(Source* s) { s->tok_start = s->cur; }

static lex_text Source_text(const Source* s)
{
    lex_text t;
    t.ptr = s->tok_start;
    t.len = (size_t)(s->cur - s->tok_start);
    return t;
}

static size_t Source_bytes(const Source* s) { return s->bytes; }
static size_t Source_col(const Source* s) { return s->col; }
static size_t Source_line(const Source* s) { return s->line; }
