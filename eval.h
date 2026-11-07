#ifndef EVAL_H
#define EVAL_H

#include "board.h"

int evaluate_board(const Board &board);

double get_game_phase(const Board &board);

int get_game_phase_int(const Board &board);

#endif
