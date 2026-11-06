#include "search.h"

#include "eval.h"
#include "tt.h"
#include "zobrist.h"

#include <algorithm>
#include <utility>
#include <vector>

using namespace std;

namespace {

constexpr int QUIESCENCE_PLY_LIMIT = 8;
constexpr int QUIESCENCE_DELTA_MARGIN = 200;
constexpr int MAX_SEARCH_PLY = 64;
constexpr int ORDER_TT_MOVE = 3000000;
constexpr int ORDER_PROMOTION = 2000000;
constexpr int ORDER_CAPTURE = 1000000;
constexpr int ORDER_KILLER_PRIMARY = 900000;
constexpr int ORDER_KILLER_SECONDARY = 800000;

Move killer_moves[MAX_SEARCH_PLY][2];

int piece_value(PieceType type) {
    switch (type) {
    case PieceType::PAWN:
        return 100;
    case PieceType::KNIGHT:
        return 320;
    case PieceType::BISHOP:
        return 330;
    case PieceType::ROOK:
        return 500;
    case PieceType::QUEEN:
        return 900;
    case PieceType::KING:
        return 10000;
    default:
        return 0;
    }
}

bool same_move(const Move &left, const Move &right) {
    return left.start_file == right.start_file &&
           left.start_rank == right.start_rank &&
           left.end_file == right.end_file &&
           left.end_rank == right.end_rank &&
           left.promotion == right.promotion;
}

bool is_en_passant_capture(Board &board, const Move &move) {
    const Piece &moving =
        board.get_piece_at(move.start_rank, move.start_file);
    if (moving.getType() != PieceType::PAWN) {
        return false;
    }
    const int end_square = move.end_rank * 8 + move.end_file;
    return board.getEnPassantSquare() == static_cast<uint8_t>(end_square);
}

bool is_capture_move(Board &board, const Move &move) {
    const Piece &target = board.get_piece_at(move.end_rank, move.end_file);
    if (target.getType() != PieceType::NONE) {
        return true;
    }
    return is_en_passant_capture(board, move);
}

bool is_promotion_move(Board &board, const Move &move) {
    if (move.promotion != PieceType::NONE) {
        return true;
    }
    const Piece &moving =
        board.get_piece_at(move.start_rank, move.start_file);
    return moving.getType() == PieceType::PAWN &&
           (move.end_rank == 0 || move.end_rank == 7);
}

bool is_tactical_move(Board &board, const Move &move) {
    return is_capture_move(board, move) || is_promotion_move(board, move);
}

bool is_quiet_underpromotion(Board &board, const Move &move) {
    if (!is_promotion_move(board, move) || is_capture_move(board, move)) {
        return false;
    }
    const PieceType promoted = (move.promotion == PieceType::NONE)
                                   ? PieceType::QUEEN
                                   : move.promotion;
    return promoted != PieceType::QUEEN;
}

int tactical_material_gain(Board &board, const Move &move) {
    int gain = 0;
    const Piece &target = board.get_piece_at(move.end_rank, move.end_file);
    if (target.getType() != PieceType::NONE) {
        gain += piece_value(target.getType());
    } else if (is_en_passant_capture(board, move)) {
        gain += piece_value(PieceType::PAWN);
    }

    if (is_promotion_move(board, move)) {
        const PieceType promoted = (move.promotion == PieceType::NONE)
                                       ? PieceType::QUEEN
                                       : move.promotion;
        gain += piece_value(promoted) - piece_value(PieceType::PAWN);
    }
    return gain;
}

int mvv_lva_score(Board &board, const Move &move) {
    const Piece &attacker =
        board.get_piece_at(move.start_rank, move.start_file);
    return tactical_material_gain(board, move) * 16 -
           piece_value(attacker.getType());
}

int promotion_order_bonus(const Move &move) {
    const PieceType promoted = (move.promotion == PieceType::NONE)
                                   ? PieceType::QUEEN
                                   : move.promotion;
    return piece_value(promoted);
}

int main_move_order_score(Board &board, const Move &move, int ply,
                          const Move *tt_move) {
    if (tt_move != nullptr && same_move(move, *tt_move)) {
        return ORDER_TT_MOVE;
    }
    if (is_promotion_move(board, move)) {
        return ORDER_PROMOTION + promotion_order_bonus(move) +
               (is_capture_move(board, move) ? mvv_lva_score(board, move)
                                             : 0);
    }
    if (is_capture_move(board, move)) {
        return ORDER_CAPTURE + mvv_lva_score(board, move);
    }
    if (ply >= 0 && ply < MAX_SEARCH_PLY) {
        if (same_move(move, killer_moves[ply][0])) {
            return ORDER_KILLER_PRIMARY;
        }
        if (same_move(move, killer_moves[ply][1])) {
            return ORDER_KILLER_SECONDARY;
        }
    }
    return 0;
}

void sort_moves_by_score(vector<pair<int, Move>> &scored_moves) {
    sort(scored_moves.begin(), scored_moves.end(),
         [](const pair<int, Move> &left, const pair<int, Move> &right) {
             return left.first > right.first;
         });
}

vector<Move> ordered_main_moves(Board &board, const vector<Move> &moves,
                                int ply, const Move *tt_move) {
    vector<pair<int, Move>> scored_moves;
    scored_moves.reserve(moves.size());
    for (const Move &move : moves) {
        scored_moves.push_back(
            {main_move_order_score(board, move, ply, tt_move), move});
    }
    sort_moves_by_score(scored_moves);

    vector<Move> ordered;
    ordered.reserve(scored_moves.size());
    for (const pair<int, Move> &entry : scored_moves) {
        ordered.push_back(entry.second);
    }
    return ordered;
}

vector<Move> ordered_tactical_moves(Board &board, const vector<Move> &moves,
                                    bool include_quiet_underpromotions) {
    vector<pair<int, Move>> scored_moves;
    scored_moves.reserve(moves.size());
    for (const Move &move : moves) {
        if (!is_tactical_move(board, move)) {
            continue;
        }
        if (!include_quiet_underpromotions &&
            is_quiet_underpromotion(board, move)) {
            continue;
        }
        scored_moves.push_back({mvv_lva_score(board, move), move});
    }
    sort_moves_by_score(scored_moves);

    vector<Move> ordered;
    ordered.reserve(scored_moves.size());
    for (const pair<int, Move> &entry : scored_moves) {
        ordered.push_back(entry.second);
    }
    return ordered;
}

void store_killer_move(int ply, const Move &move) {
    if (ply < 0 || ply >= MAX_SEARCH_PLY) {
        return;
    }
    if (same_move(move, killer_moves[ply][0])) {
        return;
    }
    killer_moves[ply][1] = killer_moves[ply][0];
    killer_moves[ply][0] = move;
}

void clear_killer_moves() {
    for (int ply = 0; ply < MAX_SEARCH_PLY; ++ply) {
        killer_moves[ply][0] = Move{};
        killer_moves[ply][1] = Move{};
    }
}

int quiescence(Board &board, int alpha, int beta, int ply, uint64_t &nodes,
               uint64_t &qnodes) {
    ++nodes;
    ++qnodes;

    const Color side = board.get_color();
    const bool in_check = board.is_check(side);
    const vector<Move> legal_moves = board.generate_all_moves(side);

    if (legal_moves.empty()) {
        if (in_check) {
            return -MATE_SCORE + ply;
        }
        return 0;
    }

    if (!in_check && ply >= QUIESCENCE_PLY_LIMIT) {
        return evaluate_board(board);
    }

    if (in_check) {
        int best_score = -SEARCH_INF;
        const vector<Move> evasions =
            ordered_main_moves(board, legal_moves, -1, nullptr);

        for (const Move &move : evasions) {
            const Piece captured =
                board.get_piece_at(move.end_rank, move.end_file);
            board.make_move(move);

            int score = 0;
            try {
                score = -quiescence(board, -beta, -alpha, ply + 1, nodes,
                                    qnodes);
            } catch (...) {
                board.undo_move(move, captured);
                throw;
            }
            board.undo_move(move, captured);

            if (score > best_score) {
                best_score = score;
            }
            if (score > alpha) {
                alpha = score;
            }
            if (alpha >= beta) {
                break;
            }
        }
        return best_score;
    }

    const int stand_pat = evaluate_board(board);
    if (stand_pat >= beta) {
        return stand_pat;
    }
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    const vector<Move> tactical_moves =
        ordered_tactical_moves(board, legal_moves, false);

    int best_score = stand_pat;
    for (const Move &move : tactical_moves) {
        const int gain = tactical_material_gain(board, move);
        if (stand_pat + gain + QUIESCENCE_DELTA_MARGIN <= alpha) {
            continue;
        }

        const Piece captured =
            board.get_piece_at(move.end_rank, move.end_file);
        board.make_move(move);

        int score = 0;
        try {
            score =
                -quiescence(board, -beta, -alpha, ply + 1, nodes, qnodes);
        } catch (...) {
            board.undo_move(move, captured);
            throw;
        }
        board.undo_move(move, captured);

        if (score > best_score) {
            best_score = score;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            break;
        }
    }

    return best_score;
}

int negamax(Board &board, int depth, int alpha, int beta, int ply,
            uint64_t &nodes, uint64_t &qnodes) {
    if (depth == 0) {
        return quiescence(board, alpha, beta, ply, nodes, qnodes);
    }

    ++nodes;

    const int original_alpha = alpha;
    const uint64_t position_key = compute_zobrist_hash(board);
    const TTLookup tt_entry = tt_probe(position_key, ply);
    if (tt_entry.found && tt_entry.depth >= depth) {
        if (tt_entry.bound == TTBound::EXACT) {
            tt_record_cutoff();
            return tt_entry.score;
        }
        if (tt_entry.bound == TTBound::LOWER && tt_entry.score >= beta) {
            tt_record_cutoff();
            return tt_entry.score;
        }
        if (tt_entry.bound == TTBound::UPPER && tt_entry.score <= alpha) {
            tt_record_cutoff();
            return tt_entry.score;
        }
    }

    const Color side = board.get_color();
    const vector<Move> legal_moves = board.generate_all_moves(side);

    if (legal_moves.empty()) {
        if (board.is_check(side)) {
            return -MATE_SCORE + ply;
        }
        return 0;
    }

    const Move *tt_move =
        (tt_entry.found && tt_entry.has_move) ? &tt_entry.best_move : nullptr;
    const vector<Move> moves =
        ordered_main_moves(board, legal_moves, ply, tt_move);
    int best_score = -SEARCH_INF;
    Move best_move = moves.front();

    for (const Move &move : moves) {
        const bool quiet_move = !is_tactical_move(board, move);
        const Piece captured = board.get_piece_at(move.end_rank, move.end_file);
        board.make_move(move);

        int score = 0;
        try {
            score = -negamax(board, depth - 1, -beta, -alpha, ply + 1, nodes,
                             qnodes);
        } catch (...) {
            board.undo_move(move, captured);
            throw;
        }

        board.undo_move(move, captured);

        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            if (quiet_move) {
                store_killer_move(ply, move);
            }
            break;
        }
    }

    TTBound bound = TTBound::EXACT;
    if (best_score <= original_alpha) {
        bound = TTBound::UPPER;
    } else if (best_score >= beta) {
        bound = TTBound::LOWER;
    }
    tt_store(position_key, depth, best_score, bound, &best_move, ply);

    return best_score;
}

} // namespace

int search_quiescence(Board &board, uint64_t &nodes, uint64_t &qnodes,
                      int ply) {
    return quiescence(board, -SEARCH_INF, SEARCH_INF, ply, nodes, qnodes);
}

SearchResult find_best_move(Board &board, int depth) {
    SearchResult result;
    result.nodes = 0;
    result.qnodes = 0;
    result.tt_probes = 0;
    result.tt_hits = 0;
    result.tt_cutoffs = 0;
    result.found_move = false;
    result.score = 0;

    clear_killer_moves();
    tt_reset_stats();

    try {
        const Color side = board.get_color();
        const vector<Move> legal_moves = board.generate_all_moves(side);

        if (legal_moves.empty()) {
            result.score = board.is_check(side) ? -MATE_SCORE : 0;
            return result;
        }

        if (depth < 1) {
            result.score = quiescence(board, -SEARCH_INF, SEARCH_INF, 0,
                                      result.nodes, result.qnodes);
            return result;
        }

        const uint64_t position_key = compute_zobrist_hash(board);
        const TTLookup tt_entry = tt_probe(position_key, 0);
        const Move *tt_move =
            (tt_entry.found && tt_entry.has_move) ? &tt_entry.best_move
                                                 : nullptr;
        const vector<Move> moves =
            ordered_main_moves(board, legal_moves, 0, tt_move);
        int alpha = -SEARCH_INF;
        const int beta = SEARCH_INF;
        int best_score = -SEARCH_INF;
        Move best_move = moves.front();

        for (const Move &move : moves) {
            const Piece captured =
                board.get_piece_at(move.end_rank, move.end_file);
            board.make_move(move);

            int score = 0;
            try {
                score = -negamax(board, depth - 1, -beta, -alpha, 1,
                                 result.nodes, result.qnodes);
            } catch (...) {
                board.undo_move(move, captured);
                throw;
            }

            board.undo_move(move, captured);

            if (score > best_score) {
                best_score = score;
                best_move = move;
            }
            if (score > alpha) {
                alpha = score;
            }
        }

        tt_store(position_key, depth, best_score, TTBound::EXACT, &best_move,
                 0);

        result.best_move = best_move;
        result.score = best_score;
        result.found_move = true;
    } catch (...) {
        result.found_move = false;
        result.score = 0;
    }

    result.tt_probes = tt_probe_count();
    result.tt_hits = tt_hit_count();
    result.tt_cutoffs = tt_cutoff_count();
    return result;
}

void clear_search_transposition_table() {
    clear_transposition_table();
}

void set_search_tt_enabled(bool enabled) {
    set_transposition_table_enabled(enabled);
}

bool search_tt_enabled() {
    return transposition_table_enabled();
}
