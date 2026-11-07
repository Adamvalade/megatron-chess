#include "zobrist.h"

namespace {

uint64_t piece_keys[2][6][64];
uint64_t castling_keys[4];
uint64_t en_passant_keys[8];
uint64_t side_to_move_key;
bool zobrist_ready = false;

uint64_t splitmix64(uint64_t &state) {
    state += 0x9E3779B97F4A7C15ULL;
    uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

bool en_passant_is_legal(const Board &board) {
    const uint8_t ep_square = board.getEnPassantSquare();
    if (ep_square == 0xFF) {
        return false;
    }

    const int ep_file = ep_square % 8;
    const int ep_rank = ep_square / 8;
    const Color side = board.get_color();

    if (side == Color::WHITE && ep_rank == 5) {
        const uint64_t pawns = board.get_bitboard(PieceType::PAWN, Color::WHITE);
        if (ep_file > 0) {
            const uint64_t left = 1ULL << ((ep_rank - 1) * 8 + (ep_file - 1));
            if (pawns & left) {
                return true;
            }
        }
        if (ep_file < 7) {
            const uint64_t right = 1ULL << ((ep_rank - 1) * 8 + (ep_file + 1));
            if (pawns & right) {
                return true;
            }
        }
    }

    if (side == Color::BLACK && ep_rank == 2) {
        const uint64_t pawns = board.get_bitboard(PieceType::PAWN, Color::BLACK);
        if (ep_file > 0) {
            const uint64_t left = 1ULL << ((ep_rank + 1) * 8 + (ep_file - 1));
            if (pawns & left) {
                return true;
            }
        }
        if (ep_file < 7) {
            const uint64_t right = 1ULL << ((ep_rank + 1) * 8 + (ep_file + 1));
            if (pawns & right) {
                return true;
            }
        }
    }

    return false;
}

} // namespace

void init_zobrist() {
    if (zobrist_ready) {
        return;
    }

    uint64_t state = 0xC0FFEE5EEDULL;
    for (int color = 0; color < 2; ++color) {
        for (int type = 0; type < 6; ++type) {
            for (int square = 0; square < 64; ++square) {
                piece_keys[color][type][square] = splitmix64(state);
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        castling_keys[i] = splitmix64(state);
    }
    for (int file = 0; file < 8; ++file) {
        en_passant_keys[file] = splitmix64(state);
    }
    side_to_move_key = splitmix64(state);
    zobrist_ready = true;
}

uint64_t compute_zobrist_hash(const Board &board) {
    init_zobrist();

    uint64_t hash = 0;
    for (Color color : {Color::WHITE, Color::BLACK}) {
        const int color_index = (color == Color::WHITE) ? 0 : 1;
        for (PieceType type :
             {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
              PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            uint64_t pieces = board.get_bitboard(type, color);
            while (pieces) {
                const int square = __builtin_ctzll(pieces);
                pieces &= pieces - 1;
                hash ^= piece_keys[color_index][static_cast<int>(type)][square];
            }
        }
    }

    if (board.get_color() == Color::BLACK) {
        hash ^= side_to_move_key;
    }

    const uint16_t rights = board.getCastlingRights();
    if (rights & WHITE_KINGSIDE) {
        hash ^= castling_keys[0];
    }
    if (rights & WHITE_QUEENSIDE) {
        hash ^= castling_keys[1];
    }
    if (rights & BLACK_KINGSIDE) {
        hash ^= castling_keys[2];
    }
    if (rights & BLACK_QUEENSIDE) {
        hash ^= castling_keys[3];
    }

    if (en_passant_is_legal(board)) {
        hash ^= en_passant_keys[board.getEnPassantSquare() % 8];
    }

    return hash;
}
