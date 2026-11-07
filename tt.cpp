#include "tt.h"

#include "search.h"

#include <cstdint>
#include <vector>

namespace {

constexpr size_t TT_ENTRY_COUNT = 1ULL << 19;

struct TTEntry {
    uint64_t key = 0;
    int32_t score = 0;
    int16_t depth = -1;
    TTBound bound = TTBound::NONE;
    int8_t start_file = 0;
    int8_t start_rank = 0;
    int8_t end_file = 0;
    int8_t end_rank = 0;
    uint8_t promotion = static_cast<uint8_t>(PieceType::NONE);
    uint8_t has_move = 0;
};

std::vector<TTEntry> table(TT_ENTRY_COUNT);
bool enabled = true;
uint64_t probes = 0;
uint64_t hits = 0;
uint64_t cutoffs = 0;

constexpr int MATE_SCORE_THRESHOLD = MATE_SCORE - 10000;

int score_to_tt(int score, int ply) {
    if (score >= MATE_SCORE_THRESHOLD) {
        return score + ply;
    }
    if (score <= -MATE_SCORE_THRESHOLD) {
        return score - ply;
    }
    return score;
}

int score_from_tt(int score, int ply) {
    if (score >= MATE_SCORE_THRESHOLD) {
        return score - ply;
    }
    if (score <= -MATE_SCORE_THRESHOLD) {
        return score + ply;
    }
    return score;
}

size_t entry_index(uint64_t key) {
    return static_cast<size_t>(key & (TT_ENTRY_COUNT - 1));
}

} // namespace

void clear_transposition_table() {
    for (TTEntry &entry : table) {
        entry = TTEntry{};
    }
    probes = 0;
    hits = 0;
    cutoffs = 0;
}

void set_transposition_table_enabled(bool value) {
    enabled = value;
}

bool transposition_table_enabled() {
    return enabled;
}

void tt_store(uint64_t key, int depth, int score, TTBound bound,
              const Move *best_move, int ply) {
    if (!enabled || depth < 0) {
        return;
    }

    TTEntry &entry = table[entry_index(key)];
    if (entry.key == key || depth >= entry.depth || entry.bound == TTBound::NONE) {
        entry.key = key;
        entry.score = score_to_tt(score, ply);
        entry.depth = static_cast<int16_t>(depth);
        entry.bound = bound;
        if (best_move != nullptr) {
            entry.start_file = static_cast<int8_t>(best_move->start_file);
            entry.start_rank = static_cast<int8_t>(best_move->start_rank);
            entry.end_file = static_cast<int8_t>(best_move->end_file);
            entry.end_rank = static_cast<int8_t>(best_move->end_rank);
            entry.promotion = static_cast<uint8_t>(best_move->promotion);
            entry.has_move = 1;
        } else {
            entry.has_move = 0;
            entry.promotion = static_cast<uint8_t>(PieceType::NONE);
        }
    }
}

TTLookup tt_probe(uint64_t key, int ply) {
    TTLookup result;
    if (!enabled) {
        return result;
    }

    ++probes;
    const TTEntry &entry = table[entry_index(key)];
    if (entry.key != key || entry.bound == TTBound::NONE) {
        return result;
    }

    ++hits;
    result.found = true;
    result.score = score_from_tt(entry.score, ply);
    result.depth = entry.depth;
    result.bound = entry.bound;
    if (entry.has_move) {
        result.has_move = true;
        result.best_move.start_file = entry.start_file;
        result.best_move.start_rank = entry.start_rank;
        result.best_move.end_file = entry.end_file;
        result.best_move.end_rank = entry.end_rank;
        result.best_move.promotion = static_cast<PieceType>(entry.promotion);
    }
    return result;
}

void tt_reset_stats() {
    probes = 0;
    hits = 0;
    cutoffs = 0;
}

uint64_t tt_probe_count() {
    return probes;
}

uint64_t tt_hit_count() {
    return hits;
}

uint64_t tt_cutoff_count() {
    return cutoffs;
}

void tt_record_cutoff() {
    ++cutoffs;
}
