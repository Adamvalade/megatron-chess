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
    void free_string(const char* s);
}


#endif