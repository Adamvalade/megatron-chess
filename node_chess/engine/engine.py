#!/usr/bin/env pypy3
from __future__ import print_function
import time, math
from itertools import count
from collections import namedtuple, defaultdict
import ctypes
import os
import sys, engine.uci as uci

version = "megatron"

print("ENGINE STARTED", flush=True)

# =========================
# Debug controls
# =========================
DEBUG_CPP = False          # high-level C++ integration logs
DEBUG_CPP_MOVES = False    # per-move detailed logs from C++

# =========================
# Optional C++ move library (get_legal_moves from node_chess/move_generator)
# Build from node_chess/move_generator: make libengine.dylib (macOS) or make libengine.so (Linux)
# =========================
_LIB = None
_lib_load_error = None
_gen_moves_backend = None  # "cpp" or "py" after gen_moves(); used to confirm which path ran

def _try_load_lib():
    global _LIB, _lib_load_error
    _engine_dir = os.path.dirname(os.path.abspath(__file__))
    _move_gen_dir = os.path.join(os.path.dirname(_engine_dir), "move_generator")
    # Prefer move_generator build so C++ fixes are used without copying the lib
    candidates = [
        os.path.join(_move_gen_dir, "libengine.dylib"),
        os.path.join(_move_gen_dir, "libengine.so"),
        os.path.join(_move_gen_dir, "libengine.dll"),
        "./libengine.dylib", "./libengine.so", "./libengine.dll",
        os.path.join(_engine_dir, "libengine.dylib"),
        os.path.join(_engine_dir, "libengine.so"),
        os.path.join(_engine_dir, "libengine.dll"),
    ]
    for path in candidates:
        try:
            if os.path.exists(path):
                if DEBUG_CPP:
                    print(f"[CPP] Trying to load: {path}", flush=True)
                _lib = ctypes.CDLL(path)
                # Define signatures (return raw pointer so we can free)
                _lib.get_legal_moves.restype  = ctypes.c_void_p
                _lib.get_legal_moves.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
                _lib.free_string.argtypes     = [ctypes.c_void_p]
                _LIB = _lib
                if DEBUG_CPP:
                    print(f"[CPP] Loaded OK: {path}", flush=True)
                return
        except OSError as e:
            _lib_load_error = e
            if DEBUG_CPP:
                print(f"[CPP] Load failed for {path}: {e}", flush=True)

_try_load_lib()
print("Move generation: C++" if _LIB else "Move generation: Python (C++ lib not loaded)", flush=True)

def _cpp_get_moves_str(board_120_str: str, color_int: int, ep_120: int = 0, castling_4: int = 15) -> str:
    """Call C++ and return the raw move string (semicolon separated), or ''."""
    if _LIB is None:
        return ""
    ptr = _LIB.get_legal_moves(board_120_str.encode('utf-8'), color_int, ep_120, castling_4)
    if not ptr:
        return ""
    try:
        raw = ctypes.cast(ptr, ctypes.c_char_p).value.decode('utf-8')
    finally:
        _LIB.free_string(ctypes.c_void_p(ptr))
    return raw or ""

# =========================
# Utilities
# =========================
def convert_board(rows8):
    """
    rows8: list of 8 strings, each 8 chars, from rank1..rank8 (A1..H1, ..., A8..H8)
    Produce a 120-char board (10x12 with borders) matching the Python engine’s 120 layout.
    """
    arr = [' '] * 120
    for r in range(8):
        for f in range(8):
            idx = 10 * (9 - r) + (f + 1)  # same indexing as classic 0x88/120 board
            arr[idx] = rows8[r][f]
    return ''.join(arr)

piece = {"P": 100, "N": 280, "B": 320, "R": 479, "Q": 929, "K": 60000}
pst = {
    'P': (0,0,0,0,0,0,0,0,78,83,86,73,102,82,85,90,7,29,21,44,40,31,44,7,-17,16,-2,15,14,0,15,-13,-26,3,10,9,6,1,0,-23,-22,9,5,-11,-10,-2,3,-19,-31,8,-7,-37,-36,-14,3,-31,0,0,0,0,0,0,0,0),
    'N': (-66,-53,-75,-75,-10,-55,-58,-70,-3,-6,100,-36,4,62,-4,-14,10,67,1,74,73,27,62,-2,24,24,45,37,33,41,25,17,-1,5,31,21,22,35,2,0,-18,10,13,22,18,15,11,-14,-23,-15,2,0,2,0,-23,-20,-74,-23,-26,-24,-19,-35,-22,-69),
    'B': (-59,-78,-82,-76,-23,-107,-37,-50,-11,20,35,-42,-39,31,2,-22,-9,39,-32,41,52,-10,28,-14,25,17,20,34,26,25,15,10,13,10,17,23,17,16,0,7,14,25,24,15,8,25,20,15,19,20,11,6,7,6,20,16,-7,2,-15,-12,-14,-15,-10,-10),
    'R': (35,29,33,4,37,33,56,50,55,29,56,67,55,62,34,60,19,35,28,33,45,27,25,15,0,5,16,13,18,-4,-9,-6,-28,-35,-16,-21,-13,-29,-46,-30,-42,-28,-42,-25,-25,-35,-26,-46,-53,-38,-31,-26,-29,-43,-44,-53,-30,-24,-18,5,-2,-18,-31,-32),
    'Q': (6,1,-8,-104,69,24,88,26,14,32,60,-10,20,76,57,24,-2,43,32,60,72,63,43,2,1,-16,22,17,25,20,-13,-6,-14,-15,-2,-5,-1,-10,-20,-22,-30,-6,-13,-11,-16,-11,-16,-27,-36,-18,0,-19,-15,-15,-21,-38,-39,-30,-31,-13,-31,-36,-34,-42),
    'K': (4,54,47,-99,-99,60,83,-62,-32,10,55,56,56,55,10,3,-62,12,-57,44,-67,28,37,-31,-55,50,11,-4,-19,13,0,-49,-55,-43,-52,-28,-51,-47,-8,-50,-47,-42,-43,-79,-64,-32,-29,-32,-4,3,-14,-50,-57,-18,13,4,17,30,-3,-14,6,-1,40,18)
}
for k, tbl in pst.items():
    padrow = lambda row: (0,) + tuple(x + piece[k] for x in row) + (0,)
    pst[k] = sum((padrow(tbl[i * 8 : i * 8 + 8]) for i in range(8)), ())
    pst[k] = (0,) * 20 + pst[k] + (0,) * 20

A1, H1, A8, H8 = 91, 98, 21, 28
initial = (
    "         \n"
    "         \n"
    " rnbqkbnr\n"
    " pppppppp\n"
    " ........\n"
    " ........\n"
    " ........\n"
    " ........\n"
    " PPPPPPPP\n"
    " RNBQKBNR\n"
    "         \n"
    "         \n"
)

N, E, S, W = -10, 1, 10, -1
directions = {
    "P": (N, N+N, N+W, N+E),
    "N": (N+N+E, E+N+E, E+S+E, S+S+E, S+S+W, W+S+W, W+N+W, N+N+W),
    "B": (N+E, S+E, S+W, N+W),
    "R": (N, E, S, W),
    "Q": (N, E, S, W, N+E, S+E, S+W, N+W),
    "K": (N, E, S, W, N+E, S+E, S+W, N+W)
}

MATE_LOWER = piece["K"] - 10 * piece["Q"]
MATE_UPPER = piece["K"] + 10 * piece["Q"]
QS = 40
QS_A = 140
EVAL_ROUGHNESS = 15
opt_ranges = dict(QS=(0,300), QS_A=(0,300), EVAL_ROUGHNESS=(0,50))

Move = namedtuple("Move", "i j prom")

class Position(namedtuple("Position", "board score wc bc ep kp")):
    # -------- Python move gen (commented out; C++ is default; uncomment body to restore) --------
    def _gen_moves_py(self):
        return  # disabled – C++ is default
        # --- original Python implementation below (uncomment to restore) ---
        for i, p in enumerate(self.board):
            if not p.isupper():
                continue
            for d in directions[p]:
                for j in count(i + d, d):
                    q = self.board[j]
                    if q.isspace() or q.isupper():
                        break
                    if p == "P":
                        if d in (N, N+N) and q != ".":
                            break
                        if d == N+N and (i < A1+N or self.board[i+N] != "."):
                            break
                        if d in (N+W, N+E) and q == "." and j not in (self.ep, self.kp, self.kp-1, self.kp+1):
                            break
                        if A8 <= j <= H8:
                            for prom in "NBRQ":
                                yield Move(i, j, prom)
                            break
                    yield Move(i, j, "")
                    if p in "PNK" or q.islower():
                        break
                    if i == A1 and self.board[j+E] == "K" and self.wc[0]:
                        yield Move(j+E, j+W, "")
                    if i == H1 and self.board[j+W] == "K" and self.wc[1]:
                        yield Move(j+W, j+E, "")

    # -------- C++ generator --------
    def _gen_moves_cpp(self):
        # C++ expects the board in standard (white's view) 120 layout: rank1 at 91-98, rank8 at 21-28.
        # When we're black to move, self.board is rotated; convert back to standard for C++.
        board_std = self.board if not self.board.startswith("\n") else (self.board[::-1].swapcase())
        rows = [
            board_std[91:99],  # rank 1 (A1..H1)
            board_std[81:89],
            board_std[71:79],
            board_std[61:69],
            board_std[51:59],
            board_std[41:49],
            board_std[31:39],
            board_std[21:29],  # rank 8 (A8..H8)
        ]
        b120 = convert_board(rows)

        # Side-to-move: rotated positions have board starting with '\n' (black to move).
        color = 1 if self.board.startswith("\n") else 0

        if DEBUG_CPP:
            # Small summary (avoid dumping whole board every time)
            up_count = sum(1 for c in self.board if c.isupper())
            lo_count = sum(1 for c in self.board if c.islower())
            print(f"[CPP] call get_legal_moves(color={color}, ep={self.ep}) | uppercase={up_count}, lowercase={lo_count}", flush=True)

        castling_4 = (1 if self.wc[1] else 0) | (2 if self.wc[0] else 0) | (4 if self.bc[1] else 0) | (8 if self.bc[0] else 0)
        raw = _cpp_get_moves_str(b120, color, self.ep, castling_4)
        if DEBUG_CPP:
            if raw:
                sample = raw.split(';')[:8]
                print(f"[CPP] raw: {len(raw)} chars | parts(sample≤8): {sample}", flush=True)
            else:
                print("[CPP] raw: <empty>", flush=True)

        if not raw:
            return

        for part in raw.split(';'):
            if not part:
                continue
            pieces = part.split(',')
            if len(pieces) < 2 or not pieces[0] or not pieces[1]:
                continue
            try:
                i = int(pieces[0]); j = int(pieces[1])
            except ValueError:
                if DEBUG_CPP_MOVES:
                    print(f"[CPP] skip non-int move: {part!r}", flush=True)
                continue

            # Sanity checks vs Python engine invariants
            if not (0 <= i < len(self.board) and 0 <= j < len(self.board)):
                if DEBUG_CPP_MOVES:
                    print(f"[CPP] out-of-range i,j=({i},{j}) len={len(self.board)}", flush=True)
                continue
            p = self.board[i]
            if not p.isupper():
                # The mover must be uppercase in this representation
                if DEBUG_CPP_MOVES:
                    print(f"[CPP] source not uppercase: i={i} '{p}'", flush=True)
                continue

            # Promotions parity identical to Python
            if p == "P" and (A8 <= j <= H8):
                for prom in "NBRQ":
                    if DEBUG_CPP_MOVES:
                        print(f"[CPP] yield prom {i}->{j}={prom}", flush=True)
                    yield Move(i, j, prom)
            else:
                if DEBUG_CPP_MOVES:
                    print(f"[CPP] yield {i}->{j}", flush=True)
                yield Move(i, j, "")

    # -------- Unified entry point --------
    # C++ move generation is the default when the lib is loaded.
    def gen_moves(self):
        global _gen_moves_backend
        if _LIB is not None:
            try:
                _gen_moves_backend = "cpp"
                for mv in self._gen_moves_cpp():
                    yield mv
                return
            except Exception as e:
                if DEBUG_CPP:
                    print(f"[CPP] exception in _gen_moves_cpp: {e!r}. Falling back to Python.", flush=True)
        _gen_moves_backend = "py"
        # Fallback to Python generator (the one that “worked perfectly”)
        for mv in self._gen_moves_py():
            yield mv

    def rotate(self, nullmove=False):
        return Position(self.board[::-1].swapcase(), -self.score, self.bc, self.wc,
                        119 - self.ep if self.ep and not nullmove else 0,
                        119 - self.kp if self.kp and not nullmove else 0)

    def move(self, mv):
        i, j, prom = mv
        p, q = self.board[i], self.board[j]
        put = lambda b, i, c: b[:i] + c + b[i+1:]
        board = self.board
        wc, bc, ep, kp = self.wc, self.bc, 0, 0
        score = self.score + self.value(mv)
        board = put(board, j, board[i])
        board = put(board, i, ".")
        if i == A1:
            wc = (False, wc[1])
        if i == H1:
            wc = (wc[0], False)
        if j == A8:
            bc = (bc[0], False)
        if j == H8:
            bc = (False, bc[1])
        if p == "K":
            wc = (False, False)
            if abs(j - i) == 2:
                kp = (i + j) // 2
                board = put(board, A1 if j < i else H1, ".")
                board = put(board, kp, "R")
        if p == "P":
            if A8 <= j <= H8:
                board = put(board, j, prom)
            if j - i == 2 * N:
                ep = i + N
            if j == self.ep:
                board = put(board, j+S, ".")
        return Position(board, score, wc, bc, ep, kp).rotate()

    def value(self, mv):
        i, j, prom = mv
        p, q = self.board[i], self.board[j]
        sc = pst[p][j] - pst[p][i]
        if q.islower():
            sc += pst[q.upper()][119 - j]
        if abs(j - self.kp) < 2:
            sc += pst["K"][119 - j]
        if p == "K" and abs(i - j) == 2:
            sc += pst["R"][(i + j) // 2]
            sc -= pst["R"][A1 if j < i else H1]
        if p == "P":
            if A8 <= j <= H8:
                sc += pst[prom][j] - pst["P"][j]
            if j == self.ep:
                sc += pst["P"][119 - (j + S)]
        return sc

Entry = namedtuple("Entry", "lower upper")

class Searcher:
    def __init__(self):
        self.tp_score = {}
        self.tp_move = {}
        self.history = set()
        self.nodes = 0
    def bound(self, pos, gamma, depth, can_null=True):
        self.nodes += 1
        depth = max(depth, 0)
        if pos.score <= -MATE_LOWER:
            return -MATE_UPPER
        entry = self.tp_score.get((pos, depth, can_null), Entry(-MATE_UPPER, MATE_UPPER))
        if entry.lower >= gamma:
            return entry.lower
        if entry.upper < gamma:
            return entry.upper
        if can_null and depth > 0 and pos in self.history:
            return 0
        def moves():
            if depth > 2 and can_null and abs(pos.score) < 500:
                yield None, -self.bound(pos.rotate(nullmove=True), 1 - gamma, depth - 3)
            if depth == 0:
                yield None, pos.score
            killer = self.tp_move.get(pos)
            if not killer and depth > 2:
                self.bound(pos, gamma, depth - 3, can_null=False)
                killer = self.tp_move.get(pos)
            val_lower = QS - depth * QS_A
            if killer and pos.value(killer) >= val_lower:
                yield killer, -self.bound(pos.move(killer), 1 - gamma, depth - 1)
            for val, mv in sorted(((pos.value(mv), mv) for mv in pos.gen_moves()), reverse=True):
                if val < val_lower:
                    break
                if depth <= 1 and pos.score + val < gamma:
                    yield mv, pos.score + val if val < MATE_LOWER else MATE_UPPER
                    break
                yield mv, -self.bound(pos.move(mv), 1 - gamma, depth - 1)
        best = -MATE_UPPER
        for mv, sc in moves():
            best = max(best, sc)
            if best >= gamma:
                if mv is not None:
                    self.tp_move[pos] = mv
                break
        if depth > 2 and best == -MATE_UPPER:
            flipped = pos.rotate(nullmove=True)
            in_check = self.bound(flipped, MATE_UPPER, 0) == MATE_UPPER
            best = -MATE_LOWER if in_check else 0
        if best >= gamma:
            self.tp_score[pos, depth, can_null] = Entry(best, entry.upper)
        if best < gamma:
            self.tp_score[pos, depth, can_null] = Entry(entry.lower, best)
        return best
    def search(self, history):
        self.nodes = 0
        self.history = set(history)
        self.tp_score.clear()
        gamma = 0
        for depth in range(1, 1000):
            lower, upper = -MATE_LOWER, MATE_UPPER
            while lower < upper - EVAL_ROUGHNESS:
                score = self.bound(history[-1], gamma, depth, can_null=False)
                if score >= gamma:
                    lower = score
                if score < gamma:
                    upper = score
                yield depth, gamma, score, self.tp_move.get(history[-1])
                gamma = (lower + upper + 1) // 2

def parse(s):
    f = ord(s[0]) - ord("a")
    r = int(s[1]) - 1
    return A1 + f - 10 * r

def render(i):
    r, f = divmod(i - A1, 10)
    return chr(f + ord("a")) + str(-r + 1)

# Hand off to the same UCI harness you were using (only when run as main)
if __name__ == "__main__":
    hist = [Position(initial, 0, (True, True), (True, True), 0, 0)]
    uci.run(sys.modules[__name__], hist[-1])
    sys.exit()
