#!/usr/bin/env python3
"""
Test for the position after 1. d4 d5 2. c4 dxc4 3. e4 Nc6 4. Nf3 (black to move).
The engine was playing Be6 (blunder) instead of e7-e6. This test checks:
- Both backends include the pawn move e7-e6 (correct) and what (i,j) they use.
- Whether C++ is mixing up or missing moves to e6.
Run from node_chess: python test_be6_position.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def main():
    import engine.engine as eng
    import engine.uci as uci
    uci.sunfish = eng

    from engine.engine import Position, initial, _LIB, render

    # Build position: 1. d4 d5 2. c4 dxc4 3. e4 Nc6 4. Nf3 (black to move)
    start = Position(initial, 0, (True, True), (True, True), 0, 0)
    uci_moves = ["d2d4", "d7d5", "c2c4", "d5c4", "e2e4", "b8c6", "g1f3"]
    hist = [start]
    for ply, uci_str in enumerate(uci_moves):
        mv = uci.parse_move(uci_str, ply % 2 == 0)
        hist.append(hist[-1].move(mv))

    pos = hist[-1]
    assert pos.board.startswith("\n"), "expected black to move (rotated board)"

    # In rotated (black) view, convention: back rank 91-98, so e7=85, e6=75 (one/two rows down).
    # But Python rotated board is built by repeated rotate() so 85 = 119-85 = 34 in "first" rotation = e7 in standard.
    # Find actual (i,j) for e7-e6: pawn on e7 moving to e6. e7 = parse('e7') in white view = 35; after one rotate 119-35=84. So e7 might be 84 or 85 depending on convention.
    # C++ outputs toIndexPython(6,4)=85 and toIndexPython(5,4)=75 for e7-e6.
    PAWN_E7_E6_CPP = (85, 75)   # what C++ sends for e7-e6
    BC8_E6 = (93, 75)
    BF8_E6 = (96, 75)

    # In rotated board e7=84, e6=74 (119-35=84, 119-45=74). Pawn e7->e6 = (84, 74).
    PAWN_E7_E6_ROTATED = (84, 74)

    def find_pawn_e7_e6(moves):
        for m in moves:
            if 0 <= m.i < len(pos.board) and pos.board[m.i] == "P":
                if m.j in (74, 75):  # e6 in rotated is 74
                    return (m.i, m.j)
        return None

    def moves_by_backend(backend):
        if backend == "py":
            return list(pos._gen_moves_py())
        return list(pos._gen_moves_cpp())

    if _LIB is None:
        print("C++ library not loaded. Build with: cd move_generator && make libengine.dylib")
        return 1

    py_moves = moves_by_backend("py")
    cpp_moves = moves_by_backend("cpp")

    py_set = set((m.i, m.j) for m in py_moves)
    cpp_set = set((m.i, m.j) for m in cpp_moves)

    # Moves to e6 (j=75) in each backend
    py_to_e6 = [(m.i, m.j) for m in py_moves if m.j == 75]
    cpp_to_e6 = [(m.i, m.j) for m in cpp_moves if m.j == 75]

    print("Position: 1. d4 d5 2. c4 dxc4 3. e4 Nc6 4. Nf3 (black to move)")
    print(f"Python moves: {len(py_moves)}  C++ moves: {len(cpp_moves)}")
    print()
    print("Moves to e6 (j=75):")
    print(f"  Python: {py_to_e6}")
    for (i, j) in py_to_e6:
        piece = pos.board[i] if 0 <= i < len(pos.board) else "?"
        print(f"    ({i},{j}) -> {render(i)}{render(j)}  piece at i: {piece!r}")
    print(f"  C++:    {cpp_to_e6}")
    for (i, j) in cpp_to_e6:
        piece = pos.board[i] if 0 <= i < len(pos.board) else "?"
        print(f"    ({i},{j}) -> {render(i)}{render(j)}  piece at i: {piece!r}")
    print()

    # Check pawn e7-e6: Python might use different index for e7 (84 vs 85)
    py_pawn_e6 = find_pawn_e7_e6(py_moves)
    cpp_pawn_e6 = find_pawn_e7_e6(cpp_moves)
    print("Pawn e7->e6 (piece P moving to j=75):")
    print(f"  Python: {py_pawn_e6}")
    print(f"  C++:    {cpp_pawn_e6}")

    print(f"  Pawn move (84,74) [e7-e6 in rotated]: Python={(84,74) in py_set} C++={(84,74) in cpp_set}")

    # Check bishop moves to e6
    for name, ij in [("Bc8-e6", BC8_E6), ("Bf8-e6", BF8_E6)]:
        in_py = ij in py_set
        in_cpp = ij in cpp_set
        print(f"  {name} {ij}: Python={in_py} C++={in_cpp}")

    # If (95,75) is in the set, that's queen to e6 - piece at 95
    if (95, 75) in cpp_set:
        print(f"  (95,75) in C++ set: piece at 95 = {pos.board[95]!r} -> would display as Be6 if 95 were bishop")

    # Symmetric difference
    only_py = py_set - cpp_set
    only_cpp = cpp_set - py_set
    if only_py or only_cpp:
        print()
        print("Move set difference:")
        if only_py:
            print(f"  Only in Python: {len(only_py)}", sorted(only_py)[:15])
        if only_cpp:
            print(f"  Only in C++:    {len(only_cpp)}", sorted(only_cpp)[:15])

    # Show piece at key indices (C++ e7=85, e6=75; Python might use 84 for e7)
    print()
    print("Piece at indices around e7/e6 (rotated board):")
    for idx in [74, 75, 76, 84, 85, 86, 94, 95, 96]:
        p = pos.board[idx] if 0 <= idx < len(pos.board) else "?"
        print(f"  [{idx}] = {p!r}")

    # Fail if move sets differ or pawn e7-e6 (84,74) missing
    errors = []
    if py_set != cpp_set:
        errors.append("C++ and Python move sets differ")
    if PAWN_E7_E6_ROTATED not in py_set:
        errors.append("Python missing pawn e7-e6 (84,74)")
    if PAWN_E7_E6_ROTATED not in cpp_set:
        errors.append("C++ missing pawn e7-e6 (84,74)")

    if errors:
        print()
        print("FAIL:", "; ".join(errors))
        return 1
    print()
    print("OK: both backends include pawn e7-e6 and move sets match")
    return 0

if __name__ == "__main__":
    sys.exit(main())
