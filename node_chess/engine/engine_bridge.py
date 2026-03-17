# engine_bridge.py
import ctypes
from pathlib import Path

# Load your shared lib
_lib = ctypes.CDLL(str(Path(__file__).with_name("libmegatron.dylib")))

# Opaque native types
class _Board(ctypes.Structure): pass
class _Piece(ctypes.Structure): pass

# POD move that matches bridge.h
class MoveC(ctypes.Structure):
    _fields_ = [
        ("start_file", ctypes.c_int),
        ("start_rank", ctypes.c_int),
        ("end_file",   ctypes.c_int),
        ("end_rank",   ctypes.c_int),
    ]

# Prototypes
_lib.board_new.restype = ctypes.POINTER(_Board)
_lib.board_delete.argtypes = [ctypes.POINTER(_Board)]

_lib.board_generate_startpos.argtypes = [ctypes.POINTER(_Board)]
_lib.board_set_turn_index.argtypes = [ctypes.POINTER(_Board), ctypes.c_int]
_lib.board_compute_attack_masks.argtypes = [ctypes.POINTER(_Board)]

_lib.board_generate_all_moves.argtypes = [
    ctypes.POINTER(_Board), ctypes.c_int, ctypes.POINTER(ctypes.POINTER(MoveC))
]
_lib.board_generate_all_moves.restype = ctypes.c_size_t

_lib.board_free_moves.argtypes = [ctypes.POINTER(MoveC)]

_lib.board_make_move.argtypes = [ctypes.POINTER(_Board), ctypes.POINTER(MoveC)]
_lib.board_make_move.restype = ctypes.POINTER(_Piece)  # opaque captured-piece handle

_lib.board_undo_move.argtypes = [
    ctypes.POINTER(_Board), ctypes.POINTER(MoveC), ctypes.POINTER(_Piece)
]
_lib.board_undo_move.restype = None

WHITE, BLACK = 0, 1  # must match bridge.h / interface.cpp

class Board:
    def __init__(self):
        self._b = _lib.board_new()
        if not self._b:
            raise RuntimeError("board_new failed")
        # Optional: ensure startpos & masks if needed
        # _lib.board_generate_startpos(self._b)
        # _lib.board_set_turn_index(self._b, 0)
        # _lib.board_compute_attack_masks(self._b)

    def __del__(self):
        if getattr(self, "_b", None):
            _lib.board_delete(self._b)
            self._b = None

    def generate_all_moves(self, color: int):
        arr_ptr = ctypes.POINTER(MoveC)()
        n = _lib.board_generate_all_moves(self._b, color, ctypes.byref(arr_ptr))
        try:
            return [arr_ptr[i] for i in range(n)]
        finally:
            if arr_ptr:
                _lib.board_free_moves(arr_ptr)

    def make_move(self, m: MoveC):
        # returns an opaque handle; store it to undo later
        cap = _lib.board_make_move(self._b, ctypes.byref(m))
        return cap  # may be NULL

    def undo_move(self, m: MoveC, captured_handle):
        _lib.board_undo_move(self._b, ctypes.byref(m), captured_handle)

# ---- Example usage ----
# b = Board()
# moves = b.generate_all_moves(WHITE)
# if moves:
#     m = moves[0]
#     cap = b.make_move(m)
#     # ... search deeper ...
#     b.undo_move(m, cap)

