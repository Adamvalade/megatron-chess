#ifndef TT_H
#define TT_H

#include "board.h"

#include <cstdint>

enum class TTBound : uint8_t {
    NONE = 0,
    EXACT = 1,
    LOWER = 2,
    UPPER = 3
};

struct TTLookup {
    bool found = false;
    int score = 0;
    int depth = -1;
    TTBound bound = TTBound::NONE;
    Move best_move{};
    bool has_move = false;
};

void clear_transposition_table();
void set_transposition_table_enabled(bool enabled);
bool transposition_table_enabled();

void tt_store(uint64_t key, int depth, int score, TTBound bound,
              const Move *best_move, int ply);
TTLookup tt_probe(uint64_t key, int ply);

void tt_reset_stats();
uint64_t tt_probe_count();
uint64_t tt_hit_count();
uint64_t tt_cutoff_count();
void tt_record_cutoff();

#endif
