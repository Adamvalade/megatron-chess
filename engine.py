#!/usr/bin/env pypy3
from __future__ import print_function
import time, math
from itertools import count
from collections import namedtuple, defaultdict
import ctypes
import sys, uci

version = "megatron"
lib = ctypes.CDLL('./../libengine.so')
lib.get_legal_moves.restype = ctypes.c_char_p
lib.get_legal_moves.argtypes = [ctypes.c_char_p, ctypes.c_int]
lib.free_string.argtypes = [ctypes.c_char_p]

def get_moves_from_cpp(board_str, turn):
    res = lib.get_legal_moves(board_str.encode('utf-8'), turn)
    moves_raw = res.decode('utf-8')
    lib.free_string(res)
    return moves_raw.split(';') if moves_raw else []

def convert_board(board):
    arr = [' ']*120
    for r in range(8):
        for f in range(8):
            idx = 10 * (9 - r) + (f + 1)
            arr[idx] = board[r][f]
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
    def gen_moves(self):
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
            lower, upper = -MATE_LOWER, MATE_LOWER
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

hist = [Position(initial, 0, (True, True), (True, True), 0, 0)]
uci.run(sys.modules[__name__], hist[-1])
sys.exit()

searcher = Searcher()
while True:
    args = input().split()
    if args[0] == "uci":
        print("id name", version)
        print("uciok")
    elif args[0] == "isready":
        print("readyok")
    elif args[0] == "quit":
        break
    elif args[:2] == ["position", "startpos"]:
        del hist[1:]
        for ply, mv in enumerate(args[3:]):
            i = parse(mv[:2])
            j = parse(mv[2:4])
            prom = mv[4:].upper()
            if ply % 2 == 1:
                i, j = 119 - i, 119 - j
            hist.append(hist[-1].move(Move(i, j, prom)))
    elif args[0] == "go":
        wtime, btime, winc, binc = [int(x)/1000 for x in args[2::2]]
        if len(hist) % 2 == 0:
            wtime, winc = btime, binc
        think = min(wtime/40 + winc, wtime/2 - 1)
        start = time.time()
        best = None
        for depth, gamma, score, mv in Searcher().search(hist):
            if score >= gamma:
                i, j = mv.i, mv.j
                if len(hist) % 2 == 0:
                    i, j = 119 - i, 119 - j
                best = render(i) + render(j) + mv.prom.lower()
                print("info depth", depth, "score cp", score, "pv", best)
            if best and time.time() - start > think * 0.8:
                break
        print("bestmove", best or '(none)')
