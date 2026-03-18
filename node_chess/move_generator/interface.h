#ifndef INTERFACE_H
#define INTERFACE_H

#include "board.h"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <algorithm>


void clearBoard(Board &board);
void loadBoard(Board &board, const char* sunfishBoard, int sideToMove);
int toIndex(int rank, int file);
bool fromIndex(int sfIndex, int &outRank, int &outFile);

extern "C" {
    /** ep_120: en passant target square in 120-index (0 = none). */
    /** castling_4: 4 bits K=1 Q=2 k=4 q=8; use 15 (0xF) to allow all inferred. */
    const char * get_legal_moves(const char* sunfishBoard, int colorInt, int ep_120, int castling_4);
    void free_string(const char* s);
}


#endif