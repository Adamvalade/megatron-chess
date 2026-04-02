#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include <unordered_map>

/** Declared in search.cpp; used by interface after make/undo sequences. */
void search_rebuild_bitboards_from_2d(Board& b);
#include <cstdint>
#include <chrono>
#include <random>

extern const int PIECE_VAL[6];

extern uint64_t ZOBRIST_PIECE[2][6][64];
extern uint64_t ZOBRIST_EP[64];
extern uint64_t ZOBRIST_CASTLE[16];
extern uint64_t ZOBRIST_SIDE;
void init_zobrist();

struct TTEntry {
    int lower;
    int upper;
    Move best_move;
    bool has_move;
};

struct TTKey {
    uint64_t hash;
    int depth;
    bool can_null;
    bool operator==(const TTKey& o) const {
        return hash == o.hash && depth == o.depth && can_null == o.can_null;
    }
};

struct TTKeyHasher {
    size_t operator()(const TTKey& k) const {
        size_t h = std::hash<uint64_t>()(k.hash);
        h ^= std::hash<int>()(k.depth) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>()(k.can_null) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

static constexpr int MATE_LOWER = 60000 - 10 * 929;
static constexpr int MATE_UPPER = 60000 + 10 * 929;
static constexpr int QS_VAL = 40;
static constexpr int QS_A_VAL = 140;
static constexpr int EVAL_ROUGHNESS = 15;

class Searcher {
public:
    Searcher();

    Move search(Board& board, Color side, int max_depth, int max_time_ms,
                int& out_score, int& out_depth, uint64_t& out_nodes);

    uint64_t nodes;

    static int pst_val(PieceType pt, Color c, int sq);
    static int piece_index(PieceType pt);
    static int compute_score(Board& board);
    static int move_value(Board& board, const Move& m, Color side);

private:
    int bound(Board& board, Color side, int gamma, int depth, bool can_null);
    uint64_t compute_hash(Board& board, Color side);

    std::unordered_map<TTKey, TTEntry, TTKeyHasher> tp_score;
    std::unordered_map<uint64_t, Move> tp_move;

    std::chrono::steady_clock::time_point search_start;
    int search_max_time_ms;
    bool time_up;
};

#endif
