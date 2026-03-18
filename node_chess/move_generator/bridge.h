#pragma once
#include <stddef.h> // size_t

#ifdef __cplusplus
extern "C" {
#endif

// Opaque native types
typedef class Board Board;
typedef class Piece Piece;

// Plain-POD move for C/Python boundary (must match your Move fields order)
typedef struct {
    int start_file; // 0..7
    int start_rank; // 0..7
    int end_file;   // 0..7
    int end_rank;   // 0..7
} MoveC;

// Lifetime
Board * board_new();
void   board_delete(Board*);

// Optional helpers (call if you need them)
void   board_generate_startpos(Board*);      // calls Board::generate_board()
void   board_set_turn_index(Board*, int ti); // 0=white, 1=black if you need parity
void   board_compute_attack_masks(Board*);   // calls Board::compute_attack_masks()

// 0 = WHITE, 1 = BLACK (matches your interface.cpp convention)
size_t board_generate_all_moves(Board*, int color, MoveC** out_moves);
// frees the array returned by board_generate_all_moves
void   board_free_moves(MoveC* arr);

// Make/undo with opaque captured-piece handle:
Piece * board_make_move(Board *, const MoveC * m);               // returns captured piece handle (or NULL)
void   board_undo_move(Board *, const MoveC * m, Piece * handle); // consumes & deletes handle

#ifdef __cplusplus
}
#endif

