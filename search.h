#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"

#include <cstdint>

constexpr int SEARCH_INF = 1000000;
constexpr int MATE_SCORE = 100000;

struct SearchResult {
    Move best_move{};
    int score = 0;
    uint64_t nodes = 0;
    uint64_t qnodes = 0;
    uint64_t tt_probes = 0;
    uint64_t tt_hits = 0;
    uint64_t tt_cutoffs = 0;
    bool found_move = false;
};

SearchResult find_best_move(Board &board, int depth);
int search_quiescence(Board &board, uint64_t &nodes, uint64_t &qnodes,
                      int ply = 0);
void clear_search_transposition_table();
void set_search_tt_enabled(bool enabled);
bool search_tt_enabled();

#endif
