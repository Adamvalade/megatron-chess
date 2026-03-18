#!/usr/bin/env python3
"""
Compare C++ vs Python move generation over a small tree of positions.
Run from node_chess: python compare_move_gen.py
Finds the first position where the move sets differ (if any).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def main():
    import engine.engine as eng
    import engine.uci as uci
    uci.sunfish = eng

    from engine.engine import (
        Position,
        initial,
        _LIB,
        render,
    )

    def move_set(pos, backend):
        """backend 'py' or 'cpp'. Returns set of (i, j, prom) from that generator."""
        if backend == "py":
            return set((m.i, m.j, m.prom) for m in pos._gen_moves_py())
        return set((m.i, m.j, m.prom) for m in pos._gen_moves_cpp())

    def compare(pos, label):
        py_set = move_set(pos, "py")
        cpp_set = move_set(pos, "cpp")
        if py_set == cpp_set:
            return True, len(py_set)
        only_py = py_set - cpp_set
        only_cpp = cpp_set - py_set
        print(f"\n=== DIFFERENCE at {label} ===", flush=True)
        print(f"  Python moves: {len(py_set)}  C++ moves: {len(cpp_set)}", flush=True)
        if only_py:
            print(f"  Only in Python ({len(only_py)}):", flush=True)
            for (i, j, prom) in sorted(only_py):
                print(f"    {render(i)}{render(j)}{prom.lower()!r}  ({i}->{j} prom={prom!r})", flush=True)
        if only_cpp:
            print(f"  Only in C++ ({len(only_cpp)}):", flush=True)
            for (i, j, prom) in sorted(only_cpp):
                print(f"    {render(i)}{render(j)}{prom.lower()!r}  ({i}->{j} prom={prom!r})", flush=True)
        return False, None

    if _LIB is None:
        print("C++ library not loaded. Build with: cd move_generator && make libengine.dylib")
        return 1

    # Build positions like UCI: position startpos moves e2e4 e7e5 ...
    start = Position(initial, 0, (True, True), (True, True), 0, 0)
    uci_moves = [
        "e2e4",
        "e7e5",
        "g1f3",
        "b8c6",
        "f1b5",
        "a7a6",
        "b5a4",
        "g8f6",
        "e1g1",  # castling
    ]
    hist = [start]
    for ply, uci_str in enumerate(uci_moves):
        mv = uci.parse_move(uci_str, ply % 2 == 0)
        hist.append(hist[-1].move(mv))

    positions = [
        ("startpos", start),
    ]
    for i, pos in enumerate(hist[1:], 1):
        positions.append((f"after {' '.join(uci_moves[:i])}", pos))

    # Position with en passant: 1. e4 e5 2. Nf3 Nc6 3. d4 exd4 -> ep = d3
    hist2 = [start]
    for ply, uci_str in enumerate(["e2e4", "e7e5", "g1f3", "b8c6", "d2d4", "e5d4"]):
        hist2.append(hist2[-1].move(uci.parse_move(uci_str, ply % 2 == 0)))
    positions.append(("after e4 e5 Nf3 Nc6 d4 exd4 (ep)", hist2[-1]))

    print("Comparing C++ vs Python move sets on sample positions...", flush=True)
    for label, pos in positions:
        ok, count = compare(pos, label)
        if ok:
            print(f"  OK {label}: {count} moves", flush=True)
        else:
            print("\nFirst difference found. Fix C++ move gen or use Python (default).", flush=True)
            return 1

    # Optional: exhaustive check up to depth N from start (every position reached)
    deep_arg = [a for a in sys.argv if a.startswith("--deep")]
    if deep_arg:
        depth_val = 4
        if "=" in deep_arg[0]:
            try:
                depth_val = int(deep_arg[0].split("=")[1])
            except ValueError:
                pass
        elif len(deep_arg) > 1:
            try:
                depth_val = int(deep_arg[1])
            except (IndexError, ValueError):
                pass
        print(f"\nDeep check (depth {depth_val} from start, following Python move order)...", flush=True)
        first_diff_path = []

        def check_depth(pos, depth, path):
            if depth == 0:
                return 0
            py_list = list(pos._gen_moves_py())
            cpp_list = list(pos._gen_moves_cpp())
            py_set = set((m.i, m.j, m.prom) for m in py_list)
            cpp_set = set((m.i, m.j, m.prom) for m in cpp_list)
            if py_set != cpp_set:
                only_py = py_set - cpp_set
                only_cpp = cpp_set - py_set
                path_str = " ".join(path) if path else "(start)"
                print(f"\n=== FIRST DIFFERENCE at depth {depth} ===", flush=True)
                print(f"  Path: {path_str}", flush=True)
                print(f"  Python moves: {len(py_set)}  C++ moves: {len(cpp_set)}", flush=True)
                if only_py:
                    print(f"  Only in Python ({len(only_py)}):", flush=True)
                    for (i, j, prom) in sorted(only_py):
                        print(f"    {render(i)}{render(j)}{prom.lower()!r}  (i={i} j={j} prom={prom!r})", flush=True)
                if only_cpp:
                    print(f"  Only in C++ ({len(only_cpp)}):", flush=True)
                    for (i, j, prom) in sorted(only_cpp):
                        print(f"    {render(i)}{render(j)}{prom.lower()!r}  (i={i} j={j} prom={prom!r})", flush=True)
                return 1
            n = 0
            for mv in py_list:
                n += check_depth(pos.move(mv), depth - 1, path + [f"{render(mv.i)}{render(mv.j)}{mv.prom.lower()}"])
                if n > 0:
                    return n
            return n
        errors = check_depth(start, depth_val, [])
        if errors:
            print(f"\nStopping after first difference.", flush=True)
            return 1
        print(f"  All positions at depth {depth_val} match.", flush=True)

    print("\nAll sampled positions match.", flush=True)
    return 0

if __name__ == "__main__":
    sys.exit(main())
