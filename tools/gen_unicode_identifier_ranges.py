#!/usr/bin/env python3

import sys

def compute_intervals(pred):
    intervals = []
    lo = None
    for cp in range(0, 0x110000):
        ok = False if 0xD800 <= cp <= 0xDFFF else pred(cp)
        if ok:
            if lo is None:
                lo = cp
        elif lo is not None:
            intervals.append((lo, cp - 1))
            lo = None
    if lo is not None:
        intervals.append((lo, 0x10FFFF))
    return intervals


def xid_start(cp):
    return cp != 0x5F and chr(cp).isidentifier()


def xid_continue(cp):
    return ("a" + chr(cp)).isidentifier()


def emit(name, ivs):
    lines = [f"    inline constexpr codepoint_interval {name}[] = {{"]
    row = []
    for lo, hi in ivs:
        row.append(f"{{0x{lo:X}, 0x{hi:X}}}")
        if len(row) == 4:
            lines.append("        " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("        " + ", ".join(row) + ",")
    lines.append("    };")
    return "\n".join(lines)


def main():
    start = compute_intervals(xid_start)
    cont = compute_intervals(xid_continue)
    print(f"XID_Start intervals: {len(start)}", file=sys.stderr)
    print(f"XID_Continue intervals: {len(cont)}", file=sys.stderr)

    print(f"""#pragma once

// Auto-generated, do not edit!
#include <cstdint>

namespace lexergen::unicode
{{
    struct codepoint_interval
    {{
        uint32_t lo, hi;
    }};

{emit("XID_START", start)}

{emit("XID_CONTINUE", cont)}
}} // namespace lexergen::unicode
""")


if __name__ == "__main__":
    main()
