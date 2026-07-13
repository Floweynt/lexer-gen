#pragma once

// Direct-threaded codegen: dfa::codegen (src/machine/dfa.cpp) builds the
// generated function body directly rather than filling in a single giant
// format string, since the per-state control flow is data-dependent. This
// header only holds the fixed boilerplate shared by every generated file.
//
// Note: lexer-gen never emits an #include for a "Source" implementation --
// that type is entirely up to the caller (see snippets/*.hpp for
// copy-pastable ones). Only <cstdint>/<cstddef>/<string_view> are needed for
// the generated function's own signature/locals.

inline constexpr auto FMT_INCLUDES = R"(#include <cstdint>
#include <cstddef>
#include <string_view>
)";
