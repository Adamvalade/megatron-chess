#ifndef INTERFACE_H
#define INTERFACE_H

#include "board.h"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <algorithm>


void clearBoard(Board &board);
void loadBoard(Board &board, const char* sunfishBoard);
int toIndex(int rank, int file);
bool fromIndex(int sfIndex, int &outRank, int &outFile);

extern "C" {
    const char * get_legal_moves(const char* sunfishBoard, int colorInt);

    /**
     * Root side to move is colorInt (0=white, 1=black) after FEN placement and optional moves.
     * fen_pieces: first FEN field, or "startpos".
     * ep_sq: en passant target 0-63, or -1 for none.
     * moves_uci: space-separated UCI moves applied after the position is loaded.
     */
    const char* search_position(const char* fen_pieces, int colorInt, int ep_sq,
                                const char* moves_uci, int max_depth, int max_time_ms);

    void free_string(const char* s);
}


#endif