#ifndef EVAL_H
#define EVAL_H

#include "board.h"

// Static position evaluation (R6).
//
// Returns the score in centipawns from the perspective of the side to
// move: positive means the side to move is better. Combines material
// (P=100, N=320, B=330, R=500, Q=900, K=0) with the piece-square tables
// in pstTables.h. No search, no king-safety/pawn-structure heuristics.
int evaluate_board(const Board &board);

// Game phase in (0, 1]: 1 at the starting position, decreasing as
// pieces are traded. Retained for future tapered (midgame/endgame)
// evaluation; not used by evaluate_board yet.
double get_game_phase(const Board &board);

#endif
