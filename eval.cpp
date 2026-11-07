#include "eval.h"

#include <algorithm>

#include "pstTables.h"

namespace {

constexpr int PIECE_VALUE[6] = {100, 320, 330, 500, 900, 0};
constexpr int PST_SCALE_DIVISOR = 10;
constexpr int MAX_PHASE = 24;
constexpr int KNIGHT_PHASE = 1;
constexpr int BISHOP_PHASE = 1;
constexpr int ROOK_PHASE = 2;
constexpr int QUEEN_PHASE = 4;

constexpr int SHELTER_MISSING_FILE = 12;
constexpr int OPEN_KING_FILE_PENALTY = 18;
constexpr int EARLY_KING_WALK_PENALTY = 22;
constexpr int CASTLING_RIGHTS_KS = 12;
constexpr int CASTLING_RIGHTS_QS = 8;

int scaled_pst(const int table[8][8], Color color, int square) {
    const int rank = square / 8;
    const int file = square % 8;
    const int row = (color == Color::WHITE) ? (7 - rank) : rank;
    return table[row][file] / PST_SCALE_DIVISOR;
}

int taper(int midgame, int endgame, int phase) {
    return (midgame * phase + endgame * (MAX_PHASE - phase)) / MAX_PHASE;
}

int material_and_pst(const Board &board, Color color, int phase) {
    int score = 0;

    for (PieceType type :
         {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
          PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
        const int value = PIECE_VALUE[static_cast<size_t>(type)];
        uint64_t pieces = board.get_bitboard(type, color);
        while (pieces) {
            const int square = __builtin_ctzll(pieces);
            pieces &= pieces - 1;
            score += value;

            int mid = 0;
            int end = 0;
            switch (type) {
                case PieceType::PAWN:
                    mid = scaled_pst(PAWN_MIDGAME, color, square);
                    end = scaled_pst(PAWN_ENDGAME, color, square);
                    break;
                case PieceType::KNIGHT:
                    mid = scaled_pst(PST_KNIGHT, color, square);
                    end = mid;
                    break;
                case PieceType::BISHOP:
                    mid = scaled_pst(PST_BISHOPS, color, square);
                    end = mid;
                    break;
                case PieceType::ROOK:
                    mid = scaled_pst(PST_ROOK, color, square);
                    end = mid;
                    break;
                case PieceType::QUEEN:
                    mid = scaled_pst(PST_QUEEN, color, square);
                    end = mid;
                    break;
                case PieceType::KING:
                    mid = scaled_pst(KING_MIDGAME, color, square);
                    end = scaled_pst(KING_ENDGAME, color, square);
                    break;
                default:
                    break;
            }
            score += taper(mid, end, phase);
        }
    }

    return score;
}

bool has_pawn_on_file_ahead(uint64_t pawns, Color color, int king_rank,
                            int file) {
    if (file < 0 || file > 7) {
        return true;
    }
    if (color == Color::WHITE) {
        const int max_rank = std::min(7, king_rank + 2);
        for (int rank = king_rank + 1; rank <= max_rank; ++rank) {
            if (pawns & (1ULL << (rank * 8 + file))) {
                return true;
            }
        }
    } else {
        const int min_rank = std::max(0, king_rank - 2);
        for (int rank = king_rank - 1; rank >= min_rank; --rank) {
            if (pawns & (1ULL << (rank * 8 + file))) {
                return true;
            }
        }
    }
    return false;
}

int king_safety_for_side(const Board &board, Color color, int phase) {
    if (phase <= 0) {
        return 0;
    }

    const uint64_t king_bb = board.get_bitboard(PieceType::KING, color);
    if (!king_bb) {
        return 0;
    }

    const int king_sq = __builtin_ctzll(king_bb);
    const int king_rank = king_sq / 8;
    const int king_file = king_sq % 8;
    const uint64_t own_pawns = board.get_bitboard(PieceType::PAWN, color);
    const int pawn_count = __builtin_popcountll(static_cast<unsigned long long>(own_pawns));
    const Color enemy =
        (color == Color::WHITE) ? Color::BLACK : Color::WHITE;
    const bool enemy_has_queen =
        board.get_bitboard(PieceType::QUEEN, enemy) != 0;
    const bool back_rank =
        (color == Color::WHITE) ? (king_rank <= 1) : (king_rank >= 6);
    const bool flank_king = king_file <= 2 || king_file >= 5;

    int penalty = 0;

    if (pawn_count >= 3 && back_rank && flank_king) {
        for (int file = king_file - 1; file <= king_file + 1; ++file) {
            if (file < 0 || file > 7) {
                continue;
            }
            if (!has_pawn_on_file_ahead(own_pawns, color, king_rank, file)) {
                penalty += SHELTER_MISSING_FILE;
            }
        }
    }

    if (enemy_has_queen && back_rank && king_file >= 3 && king_file <= 4) {
        penalty += OPEN_KING_FILE_PENALTY;
    }

    const uint16_t rights = board.getCastlingRights();
    const bool on_start_file = king_file == 4;
    const bool on_start_rank =
        (color == Color::WHITE) ? (king_rank == 0) : (king_rank == 7);
    bool has_rights = false;
    if (color == Color::WHITE) {
        has_rights = (rights & (WHITE_KINGSIDE | WHITE_QUEENSIDE)) != 0;
    } else {
        has_rights = (rights & (BLACK_KINGSIDE | BLACK_QUEENSIDE)) != 0;
    }

    if (enemy_has_queen && on_start_file && !on_start_rank && !has_rights) {
        penalty += EARLY_KING_WALK_PENALTY;
    }

    int rights_bonus = 0;
    if (enemy_has_queen && on_start_rank && on_start_file) {
        if (color == Color::WHITE) {
            if (rights & WHITE_KINGSIDE) {
                rights_bonus += CASTLING_RIGHTS_KS;
            }
            if (rights & WHITE_QUEENSIDE) {
                rights_bonus += CASTLING_RIGHTS_QS;
            }
        } else {
            if (rights & BLACK_KINGSIDE) {
                rights_bonus += CASTLING_RIGHTS_KS;
            }
            if (rights & BLACK_QUEENSIDE) {
                rights_bonus += CASTLING_RIGHTS_QS;
            }
        }
    }

    const int net = rights_bonus - penalty;
    return taper(net, 0, phase);
}

} // namespace

int get_game_phase_int(const Board &board) {
    int phase = 0;
    for (Color color : {Color::WHITE, Color::BLACK}) {
        phase += KNIGHT_PHASE * __builtin_popcountll(static_cast<unsigned long long>(
            board.get_bitboard(PieceType::KNIGHT, color)));
        phase += BISHOP_PHASE * __builtin_popcountll(static_cast<unsigned long long>(
            board.get_bitboard(PieceType::BISHOP, color)));
        phase += ROOK_PHASE * __builtin_popcountll(static_cast<unsigned long long>(
            board.get_bitboard(PieceType::ROOK, color)));
        phase += QUEEN_PHASE * __builtin_popcountll(static_cast<unsigned long long>(
            board.get_bitboard(PieceType::QUEEN, color)));
    }
    return std::min(phase, MAX_PHASE);
}

double get_game_phase(const Board &board) {
    return get_game_phase_int(board) / double(MAX_PHASE);
}

int evaluate_board(const Board &board) {
    const int phase = get_game_phase_int(board);
    const int white =
        material_and_pst(board, Color::WHITE, phase) +
        king_safety_for_side(board, Color::WHITE, phase);
    const int black =
        material_and_pst(board, Color::BLACK, phase) +
        king_safety_for_side(board, Color::BLACK, phase);
    const int white_perspective = white - black;
    return (board.get_color() == Color::WHITE) ? white_perspective
                                               : -white_perspective;
}
