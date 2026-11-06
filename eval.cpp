#include "eval.h"

#include <algorithm>

#include "pstTables.h"

namespace {

// Material values in centipawns, indexed by PieceType
// (PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING). The king has no material
// value: checkmate is handled by search, not evaluation.
constexpr int PIECE_VALUE[6] = {100, 320, 330, 500, 900, 0};

// Piece-square tables from pstTables.h (midgame tables; the file's
// pawn/king endgame tables are reserved for a future tapered eval).
// The tables are written from White's point of view with row 0 = rank 8
// and row 7 = rank 1 (the usual printed-board orientation).
const int (*table_for(PieceType type))[8] {
    switch (type) {
        case PieceType::PAWN:   return PAWN_MIDGAME;
        case PieceType::KNIGHT: return PST_KNIGHT;
        case PieceType::BISHOP: return PST_BISHOPS;
        case PieceType::ROOK:   return PST_ROOK;
        case PieceType::QUEEN:  return PST_QUEEN;
        case PieceType::KING:   return KING_MIDGAME;
        default:                return nullptr;
    }
}

// PST value of one piece. White pieces index the table as
// [7 - rank][file]; Black pieces are mirrored vertically as
// [rank][file], so both colors get the same bonuses for equivalent
// squares (e.g. a white knight on d4 and a black knight on d5).
constexpr int PST_SCALE_DIVISOR = 10;

int pst_value(const int table[8][8], Color color, int square) {
    const int rank = square / 8;
    const int file = square % 8;
    const int row = (color == Color::WHITE) ? (7 - rank) : rank;
    return table[row][file] / PST_SCALE_DIVISOR;
}

// Material + PST score of one color, in centipawns. Iterates the
// board's bitboards directly: no allocations, no board mutation.
int score_side(const Board &board, Color color) {
    int score = 0;
    for (PieceType type :
         {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
          PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
        const int (*table)[8] = table_for(type);
        const int value =
            PIECE_VALUE[static_cast<size_t>(type)];

        uint64_t pieces = board.get_bitboard(type, color);
        while (pieces) {
            const int square = __builtin_ctzll(pieces);
            pieces &= pieces - 1;
            score += value + pst_value(table, color, square);
        }
    }
    return score;
}

} // namespace

int evaluate_board(const Board &board) {
    // Score from White's perspective, then convert to the side to move.
    const int white_perspective =
        score_side(board, Color::WHITE) - score_side(board, Color::BLACK);
    return (board.get_color() == Color::WHITE) ? white_perspective
                                               : -white_perspective;
}

double get_game_phase(const Board &board) {
    // Phase weights for non-pawn pieces
    constexpr int KNIGHT_PHASE = 1;
    constexpr int BISHOP_PHASE = 1;
    constexpr int ROOK_PHASE = 2;
    constexpr int QUEEN_PHASE = 4;

    // Total phase points in the starting position
    constexpr int MAX_PHASE = 24;

    int phase = 0;

    for (Color color : {Color::WHITE, Color::BLACK}) {
        phase += KNIGHT_PHASE * __builtin_popcountll(
            board.get_bitboard(PieceType::KNIGHT, color));

        phase += BISHOP_PHASE * __builtin_popcountll(
            board.get_bitboard(PieceType::BISHOP, color));

        phase += ROOK_PHASE * __builtin_popcountll(
            board.get_bitboard(PieceType::ROOK, color));

        phase += QUEEN_PHASE * __builtin_popcountll(
            board.get_bitboard(PieceType::QUEEN, color));
    }

    return std::min(phase, MAX_PHASE) / double(MAX_PHASE);
}
