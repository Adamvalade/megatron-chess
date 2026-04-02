#!/usr/bin/env python3
"""Smoke test: C++ search via ctypes (requires move_generator/libengine built with search_position)."""
import os
import sys

# Repo layout: node_chess/tests/ -> node_chess on path
_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)


def main() -> int:
    import engine.engine as eng

    if eng._LIB is None or not hasattr(eng._LIB, "search_position"):
        print("skip: libengine without search_position")
        return 0
    r = eng.cpp_search(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
        0,
        -1,
        "",
        4,
        5000,
    )
    if not r or r[0] in ("0000", ""):
        print("FAIL: cpp_search:", r)
        return 1
    move, score, depth, nodes = r
    print("ok cpp_search", move, "score", score, "depth", depth, "nodes", nodes)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
