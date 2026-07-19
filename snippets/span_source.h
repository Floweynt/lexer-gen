#include <stddef.h>
#include <stdint.h>

typedef struct
{
    const char* cur;
    const char* la;
    const char* end;
    const char* tok_start;

    size_t bytes;
    size_t la_bytes;
} Source;

static void Source_init(Source* s, const char* begin, const char* end)
{
    s->cur = begin;
    s->la = begin;
    s->end = end;
    s->tok_start = begin;
    s->bytes = 0;
    s->la_bytes = 0;
}

static int Source_peek(Source* s)
{
    if (s->la >= s->end)
    {
        return 0;
    }

    unsigned char ch = (unsigned char)*s->la++;
    s->la_bytes++;

    return ch;
}

static void Source_accept(Source* s)
{
    s->cur = s->la;
    s->bytes = s->la_bytes;
}

static void Source_backtrack(Source* s)
{
    s->la = s->cur;
    s->la_bytes = s->bytes;
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

#define LEXGEN_C_SOURCE_HAS_SCAN 1

static lex_text Source_remaining(const Source* s)
{
    lex_text t;
    t.ptr = s->la;
    t.len = (size_t)(s->end - s->la);
    return t;
}

static void Source_skip(Source* s, size_t n)
{
    s->la += n;
    s->la_bytes += n;
}
