#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "board.h"

#include <cstdint>

void init_zobrist();
uint64_t compute_zobrist_hash(const Board &board);

#endif
