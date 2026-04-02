#include "search.h"
#include <algorithm>
#include <cstring>
#include <iostream>

// Friend of Board: resync bitboards from 8x8 piece array (undo_move can desync aggregates).
void search_rebuild_bitboards_from_2d(Board& b) {
    b.white_pawns = b.black_pawns = b.white_knights = b.black_knights =
    b.white_rooks = b.black_rooks = b.white_bishops = b.black_bishops =
    b.white_queens = b.black_queens = b.white_kings = b.black_kings = 0ULL;

    for (int r = 0; r < 8; r++) {
        for (int f = 0; f < 8; f++) {
            const Piece& p = b.board[static_cast<size_t>(r)][static_cast<size_t>(f)];
            if (p.getType() == PieceType::NONE) continue;
            uint64_t bit = 1ULL << (r * 8 + f);
            if (p.getColor() == Color::WHITE) {
                switch (p.getType()) {
                    case PieceType::PAWN:   b.white_pawns   |= bit; break;
                    case PieceType::KNIGHT: b.white_knights |= bit; break;
                    case PieceType::BISHOP: b.white_bishops |= bit; break;
                    case PieceType::ROOK:   b.white_rooks   |= bit; break;
                    case PieceType::QUEEN:  b.white_queens  |= bit; break;
                    case PieceType::KING:   b.white_kings   |= bit; break;
                    default: break;
                }
            } else {
                switch (p.getType()) {
                    case PieceType::PAWN:   b.black_pawns   |= bit; break;
                    case PieceType::KNIGHT: b.black_knights |= bit; break;
                    case PieceType::BISHOP: b.black_bishops |= bit; break;
                    case PieceType::ROOK:   b.black_rooks   |= bit; break;
                    case PieceType::QUEEN:  b.black_queens  |= bit; break;
                    case PieceType::KING:   b.black_kings   |= bit; break;
                    default: break;
                }
            }
        }
    }
    b.all_white_pieces = b.white_pawns | b.white_knights | b.white_bishops
                         | b.white_rooks | b.white_queens | b.white_kings;
    b.all_black_pieces = b.black_pawns | b.black_knights | b.black_bishops
                         | b.black_rooks | b.black_queens | b.black_kings;
    b.all_pieces = b.all_white_pieces | b.all_black_pieces;
}

const int PIECE_VAL[6] = {100, 280, 320, 479, 929, 60000};

// PST raw values: row 0 = rank 8 (top), row 7 = rank 1 (bottom)
static const int PST_RAW[6][64] = {
    // PAWN
    {   0,  0,  0,  0,  0,  0,  0,  0,
       78, 83, 86, 73,102, 82, 85, 90,
        7, 29, 21, 44, 40, 31, 44,  7,
      -17, 16, -2, 15, 14,  0, 15,-13,
      -26,  3, 10,  9,  6,  1,  0,-23,
      -22,  9,  5,-11,-10, -2,  3,-19,
      -31,  8, -7,-37,-36,-14,  3,-31,
        0,  0,  0,  0,  0,  0,  0,  0},
    // KNIGHT
    { -66,-53,-75,-75,-10,-55,-58,-70,
       -3, -6,100,-36,  4, 62, -4,-14,
       10, 67,  1, 74, 73, 27, 62, -2,
       24, 24, 45, 37, 33, 41, 25, 17,
       -1,  5, 31, 21, 22, 35,  2,  0,
      -18, 10, 13, 22, 18, 15, 11,-14,
      -23,-15,  2,  0,  2,  0,-23,-20,
      -74,-23,-26,-24,-19,-35,-22,-69},
    // BISHOP
    { -59,-78,-82,-76,-23,-107,-37,-50,
      -11, 20, 35,-42,-39, 31,  2,-22,
       -9, 39,-32, 41, 52,-10, 28,-14,
       25, 17, 20, 34, 26, 25, 15, 10,
       13, 10, 17, 23, 17, 16,  0,  7,
       14, 25, 24, 15,  8, 25, 20, 15,
       19, 20, 11,  6,  7,  6, 20, 16,
       -7,  2,-15,-12,-14,-15,-10,-10},
    // ROOK
    {  35, 29, 33,  4, 37, 33, 56, 50,
       55, 29, 56, 67, 55, 62, 34, 60,
       19, 35, 28, 33, 45, 27, 25, 15,
        0,  5, 16, 13, 18, -4, -9, -6,
      -28,-35,-16,-21,-13,-29,-46,-30,
      -42,-28,-42,-25,-25,-35,-26,-46,
      -53,-38,-31,-26,-29,-43,-44,-53,
      -30,-24,-18,  5, -2,-18,-31,-32},
    // QUEEN
    {   6,  1, -8,-104, 69, 24, 88, 26,
       14, 32, 60,-10, 20, 76, 57, 24,
       -2, 43, 32, 60, 72, 63, 43,  2,
        1,-16, 22, 17, 25, 20,-13, -6,
      -14,-15, -2, -5, -1,-10,-20,-22,
      -30, -6,-13,-11,-16,-11,-16,-27,
      -36,-18,  0,-19,-15,-15,-21,-38,
      -39,-30,-31,-13,-31,-36,-34,-42},
    // KING
    {   4, 54, 47,-99,-99, 60, 83,-62,
      -32, 10, 55, 56, 56, 55, 10,  3,
      -62, 12,-57, 44,-67, 28, 37,-31,
      -55, 50, 11, -4,-19, 13,  0,-49,
      -55,-43,-52,-28,-51,-47, -8,-50,
      -47,-42,-43,-79,-64,-32,-29,-32,
       -4,  3,-14,-50,-57,-18, 13,  4,
       17, 30, -3,-14,  6, -1, 40, 18}
};

static int pst_computed[6][64];
static bool pst_initialized = false;

static void init_pst() {
    if (pst_initialized) return;
    for (int pt = 0; pt < 6; pt++) {
        for (int sq = 0; sq < 64; sq++) {
            pst_computed[pt][sq] = PIECE_VAL[pt] + PST_RAW[pt][sq];
        }
    }
    pst_initialized = true;
}

uint64_t ZOBRIST_PIECE[2][6][64];
uint64_t ZOBRIST_EP[64];
uint64_t ZOBRIST_CASTLE[16];
uint64_t ZOBRIST_SIDE;
static bool zobrist_initialized = false;

void init_zobrist() {
    if (zobrist_initialized) return;
    std::mt19937_64 rng(0xDEADBEEF42ULL);
    for (int c = 0; c < 2; c++)
        for (int pt = 0; pt < 6; pt++)
            for (int sq = 0; sq < 64; sq++)
                ZOBRIST_PIECE[c][pt][sq] = rng();
    for (int i = 0; i < 64; i++)
        ZOBRIST_EP[i] = rng();
    for (int i = 0; i < 16; i++)
        ZOBRIST_CASTLE[i] = rng();
    ZOBRIST_SIDE = rng();
    zobrist_initialized = true;
}

Searcher::Searcher() : nodes(0), search_max_time_ms(0), time_up(false) {
    init_pst();
    init_zobrist();
}

int Searcher::piece_index(PieceType pt) {
    switch (pt) {
        case PieceType::PAWN:   return 0;
        case PieceType::KNIGHT: return 1;
        case PieceType::BISHOP: return 2;
        case PieceType::ROOK:   return 3;
        case PieceType::QUEEN:  return 4;
        case PieceType::KING:   return 5;
        default: return -1;
    }
}

// PST value for a piece on a square.
// PST_RAW row 0 = rank 8; squares use rank*8+file where rank 0 = rank 1.
// For the piece's "own" perspective: White pieces use (7-rank)*8+file,
// Black pieces use rank*8+file (mirrored).
int Searcher::pst_val(PieceType pt, Color c, int sq) {
    int pi = piece_index(pt);
    if (pi < 0) return 0;
    int rank = sq / 8, file = sq % 8;
    int idx = (c == Color::WHITE) ? ((7 - rank) * 8 + file) : (rank * 8 + file);
    return pst_computed[pi][idx];
}

// Score from White's perspective (positive = White ahead)
int Searcher::compute_score(Board& board) {
    int score = 0;
    for (int r = 0; r < 8; r++) {
        for (int f = 0; f < 8; f++) {
            const Piece& p = board.get_piece_at(r, f);
            if (p.getType() == PieceType::NONE) continue;
            int sq = r * 8 + f;
            int val = pst_val(p.getType(), p.getColor(), sq);
            score += (p.getColor() == Color::WHITE) ? val : -val;
        }
    }
    return score;
}

// Move value for ordering (from the mover's perspective)
int Searcher::move_value(Board& board, const Move& m, Color side) {
    const Piece& mover = board.get_piece_at(m.start_rank, m.start_file);
    const Piece& target = board.get_piece_at(m.end_rank, m.end_file);
    int from_sq = m.start_rank * 8 + m.start_file;
    int to_sq = m.end_rank * 8 + m.end_file;

    int sc = pst_val(mover.getType(), side, to_sq) - pst_val(mover.getType(), side, from_sq);

    if (target.getType() != PieceType::NONE) {
        Color opp = (side == Color::WHITE) ? Color::BLACK : Color::WHITE;
        sc += pst_val(target.getType(), opp, to_sq);
    }

    if (mover.getType() == PieceType::PAWN) {
        if ((side == Color::WHITE && m.end_rank == 7) ||
            (side == Color::BLACK && m.end_rank == 0)) {
            sc += pst_val(PieceType::QUEEN, side, to_sq) - pst_val(PieceType::PAWN, side, to_sq);
        }
    }

    return sc;
}

uint64_t Searcher::compute_hash(Board& board, Color side) {
    uint64_t h = 0;
    for (int r = 0; r < 8; r++) {
        for (int f = 0; f < 8; f++) {
            const Piece& p = board.get_piece_at(r, f);
            if (p.getType() == PieceType::NONE) continue;
            int ci = (p.getColor() == Color::WHITE) ? 0 : 1;
            int pi = piece_index(p.getType());
            int sq = r * 8 + f;
            h ^= ZOBRIST_PIECE[ci][pi][sq];
        }
    }
    uint8_t ep8 = board.getEnPassantSquare();
    if (ep8 < 64) h ^= ZOBRIST_EP[ep8];
    h ^= ZOBRIST_CASTLE[board.getCastlingRights()];
    if (side == Color::BLACK) h ^= ZOBRIST_SIDE;
    return h;
}

// bound() — faithful port of Python Sunfish's bound()
// `side` is the side to move; score from that side's perspective.
int Searcher::bound(Board& board, Color side, int gamma, int depth, bool can_null) {
    nodes++;

    if (time_up) return 0;
    if (search_max_time_ms > 0 && (nodes & 4095) == 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start).count();
        if (elapsed >= search_max_time_ms) {
            time_up = true;
            return 0;
        }
    }

    depth = std::max(depth, 0);

    // Score from mover's perspective
    int score = compute_score(board);
    int eval = (side == Color::WHITE) ? score : -score;

    if (eval <= -MATE_LOWER) return -MATE_UPPER;

    uint64_t pos_hash = compute_hash(board, side);
    TTKey key{pos_hash, depth, can_null};

    auto it = tp_score.find(key);
    int entry_lower = -MATE_UPPER, entry_upper = MATE_UPPER;
    if (it != tp_score.end()) {
        entry_lower = it->second.lower;
        entry_upper = it->second.upper;
        if (entry_lower >= gamma) return entry_lower;
        if (entry_upper < gamma) return entry_upper;
    }

    int best = -MATE_UPPER;
    Move best_move{-1, -1, -1, -1};
    bool found_move = false;
    Color opp = (side == Color::WHITE) ? Color::BLACK : Color::WHITE;

    // --- Null move ---
    if (depth > 2 && can_null && std::abs(eval) < 500) {
        int null_sc = -bound(board, opp, 1 - gamma, depth - 3, false);
        if (time_up) return 0;
        if (null_sc > best) best = null_sc;
        if (best >= gamma) {
            // null move cutoff (no move to store)
            tp_score[key] = {best, entry_upper, {}, false};
            return best;
        }
    }

    // --- Static eval at depth 0 ---
    if (depth == 0) {
        if (eval > best) best = eval;
        if (best >= gamma) {
            tp_score[key] = {best, entry_upper, {}, false};
            return best;
        }
    }

    int val_lower = QS_VAL - depth * QS_A_VAL;

    // --- Killer move from TT ---
    auto kit = tp_move.find(pos_hash);
    Move killer{-1, -1, -1, -1};
    bool has_killer = (kit != tp_move.end());
    if (has_killer) killer = kit->second;

    if (!has_killer && depth > 2) {
        bound(board, side, gamma, depth - 3, false);
        if (time_up) return 0;
        kit = tp_move.find(pos_hash);
        has_killer = (kit != tp_move.end());
        if (has_killer) killer = kit->second;
    }

    // --- Try killer move first ---
    if (has_killer && move_value(board, killer, side) >= val_lower) {
        Piece cap = board.get_piece_at(killer.end_rank, killer.end_file);
        board.make_move(killer);
        int s = -bound(board, opp, 1 - gamma, depth - 1, true);
        board.undo_move(killer, cap);
        search_rebuild_bitboards_from_2d(board);
        if (time_up) return 0;

        if (s > best) {
            best = s;
            best_move = killer;
            found_move = true;
        }
        if (best >= gamma) {
            tp_move[pos_hash] = best_move;
            tp_score[key] = {best, entry_upper, best_move, true};
            return best;
        }
    }

    // --- Generate and sort regular moves ---
    search_rebuild_bitboards_from_2d(board);
    std::vector<Move> moves = board.generate_all_moves(side);
    std::vector<std::pair<int, size_t>> move_vals;
    move_vals.reserve(moves.size());
    for (size_t i = 0; i < moves.size(); i++) {
        move_vals.push_back({move_value(board, moves[i], side), i});
    }
    std::sort(move_vals.begin(), move_vals.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (const auto& [val, idx] : move_vals) {
        const Move& m = moves[idx];

        // Skip killer (already tried)
        if (has_killer && m.start_rank == killer.start_rank && m.start_file == killer.start_file &&
            m.end_rank == killer.end_rank && m.end_file == killer.end_file)
            continue;

        if (val < val_lower) break;

        // Futility at shallow depth
        if (depth <= 1 && eval + val < gamma) {
            int futility = (val < MATE_LOWER) ? (eval + val) : MATE_UPPER;
            if (futility > best) {
                best = futility;
                best_move = m;
                found_move = true;
            }
            break;
        }

        Piece cap = board.get_piece_at(m.end_rank, m.end_file);
        board.make_move(m);
        int s = -bound(board, opp, 1 - gamma, depth - 1, true);
        board.undo_move(m, cap);
        search_rebuild_bitboards_from_2d(board);
        if (time_up) return 0;

        if (s > best) {
            best = s;
            best_move = m;
            found_move = true;
        }
        if (best >= gamma) {
            tp_move[pos_hash] = best_move;
            tp_score[key] = {best, entry_upper, best_move, true};
            return best;
        }
    }

    // --- Checkmate / stalemate ---
    if (depth > 2 && best == -MATE_UPPER) {
        // Check if side is in check (= checkmate) or not (= stalemate)
        bool in_check = board.is_check(side);
        best = in_check ? -MATE_LOWER : 0;
    }

    // Store TT
    if (best >= gamma) {
        tp_score[key] = {best, entry_upper, best_move, found_move};
    } else {
        tp_score[key] = {entry_lower, best, best_move, found_move};
    }
    if (found_move && best_move.start_rank >= 0) {
        tp_move[pos_hash] = best_move;
    }

    return best;
}

Move Searcher::search(Board& board, Color side, int max_depth, int max_time_ms,
                      int& out_score, int& out_depth, uint64_t& out_nodes) {
    nodes = 0;
    tp_score.clear();
    time_up = false;
    search_max_time_ms = max_time_ms;
    search_start = std::chrono::steady_clock::now();

    if (max_depth <= 0) max_depth = 100;

    Move best_move{-1, -1, -1, -1};
    int best_score = 0;
    int best_depth = 0;

    for (int depth = 1; depth <= max_depth; depth++) {
        int gamma = 0;
        int lower = -MATE_LOWER, upper = MATE_UPPER;
        while (lower < upper - EVAL_ROUGHNESS) {
            if (time_up) goto done;

            int score = bound(board, side, gamma, depth, false);
            if (time_up) goto done;

            if (score >= gamma) {
                lower = score;
            } else {
                upper = score;
            }

            gamma = (lower + upper + 1) / 2;
        }

        // Update best from TT
        uint64_t h = compute_hash(board, side);
        auto mit = tp_move.find(h);
        if (mit != tp_move.end()) {
            best_move = mit->second;
            best_score = lower;
            best_depth = depth;
        }

        // Time management: if > 2/3 of time used, stop
        if (max_time_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start).count();
            if (elapsed >= max_time_ms * 2 / 3) break;
        }
    }

done:
    out_score = best_score;
    out_depth = best_depth;
    out_nodes = nodes;
    return best_move;
}
