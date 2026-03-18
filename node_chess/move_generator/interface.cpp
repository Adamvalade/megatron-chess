#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include "board.h" 
#include "interface.h"



void clearBoard(Board & board) {
    board.clear();
}


int toIndex(int rank, int file) {
    return 10 * (9 - rank) + (1 + file);
}

// When black to move, Python uses rotated board: square at standard index s is at 119-s.
// So output 119 - toIndex(rank, file) so the move indices match position.board.
static int toIndexRotated(int rank, int file) {
    return 119 - toIndex(rank, file);
}


bool fromIndex(int sfIndex, int &outRank, int &outFile)
{
    int row = sfIndex / 10;
    int col = sfIndex % 10;

    if (row < 2 || row > 9) return false;
    if (col < 1 || col > 8) return false;

    outRank = 9 - row;  
    outFile = col - 1;
    return true;
}

void loadBoard(Board & board, const char* b, int /* sideToMove */)
{
    // Python always sends standard (white's view) 120 layout: b[i] = piece at standard square i.
    for (int i = 0; i < 120; i++) {
        char pc = b[i];
        if (pc == '.' || pc == ' ' || pc == '\n' || pc == '\r' || pc == '\t') {
            continue;
        }
        if (pc == '\0') break;

        int rank, file;
        if (!fromIndex(i, rank, file)) {
            continue;
        }

        // Standard layout: white = uppercase, black = lowercase.
        bool isWhite = (std::isupper(pc) != 0);
        char base = (char)std::toupper(pc);

        Color c = isWhite ? Color::WHITE : Color::BLACK;
        PieceType pt = PieceType::NONE;
        switch (base) {
            case 'P': pt = PieceType::PAWN;   break;
            case 'N': pt = PieceType::KNIGHT; break;
            case 'B': pt = PieceType::BISHOP; break;
            case 'R': pt = PieceType::ROOK;   break;
            case 'Q': pt = PieceType::QUEEN;  break;
            case 'K': pt = PieceType::KING;   break;
            default:  pt = PieceType::NONE;   break;
        }

        if (pt != PieceType::NONE) {
            board.board[static_cast<size_t>(rank)][static_cast<size_t>(file)] = Piece(pt, c);
        }
    }

    board.white_pawns = board.white_knights = board.white_bishops = board.white_rooks =
    board.white_queens = board.white_kings = 0ULL;
    board.black_pawns = board.black_knights = board.black_bishops = board.black_rooks =
    board.black_queens = board.black_kings = 0ULL;
    board.all_white_pieces = 0ULL;
    board.all_black_pieces = 0ULL;
    board.all_pieces       = 0ULL;

    for (int r = 0; r < 8; r++) {
        for (int f = 0; f < 8; f++) {
            Piece p = board.board[static_cast<size_t>(r)][static_cast<size_t>(f)];
            if (p.getType() == PieceType::NONE) continue;
            uint64_t bit = 1ULL << (r*8 + f);  
            if (p.getColor() == Color::WHITE) {
                switch (p.getType()) {
                    case PieceType::PAWN:   board.white_pawns   |= bit; break;
                    case PieceType::KNIGHT: board.white_knights |= bit; break;
                    case PieceType::BISHOP: board.white_bishops |= bit; break;
                    case PieceType::ROOK:   board.white_rooks   |= bit; break;
                    case PieceType::QUEEN:  board.white_queens  |= bit; break;
                    case PieceType::KING:   board.white_kings   |= bit; break;
                    default: break;
                }
            } else {
                switch (p.getType()) {
                    case PieceType::PAWN:   board.black_pawns   |= bit; break;
                    case PieceType::KNIGHT: board.black_knights |= bit; break;
                    case PieceType::BISHOP: board.black_bishops |= bit; break;
                    case PieceType::ROOK:   board.black_rooks   |= bit; break;
                    case PieceType::QUEEN:  board.black_queens  |= bit; break;
                    case PieceType::KING:   board.black_kings   |= bit; break;
                    default: break;
                }
            }
        }
    }

    board.all_white_pieces = board.white_pawns | board.white_knights | board.white_bishops
                             | board.white_rooks | board.white_queens | board.white_kings;
    board.all_black_pieces = board.black_pawns | board.black_knights | board.black_bishops
                             | board.black_rooks | board.black_queens | board.black_kings;
    board.all_pieces       = board.all_white_pieces | board.all_black_pieces;

}



const char * get_legal_moves(const char* sunfishBoard, int colorInt, int ep_120, int castling_4)
{
    Board board;
    clearBoard(board);
    Color sideToMove = (colorInt == 0) ? Color::WHITE : Color::BLACK;
    loadBoard(board, sunfishBoard, colorInt);

    board.set_castling_mask(static_cast<uint16_t>((castling_4 >= 0 && castling_4 <= 15) ? castling_4 : 0x0F));

    if (ep_120 > 0) {
        int rank, file;
        // When black to move, Python sends ep in rotated index space (119 - standard).
        int ep_standard = (colorInt == 1) ? (119 - ep_120) : ep_120;
        if (fromIndex(ep_standard, rank, file)) {
            int sq = rank * 8 + file;
            board.set_en_passant(sq);
        } else {
            board.set_en_passant(64);
        }
    } else {
        board.set_en_passant(64);
    }

    if (sideToMove == Color::WHITE) {
        board.set_turn_index(0);
    } else {
        board.set_turn_index(1);
    }

    board.compute_attack_masks();

    std::vector<Move> moves = board.generate_all_moves(sideToMove);
    std::ostringstream oss;
    for (size_t m = 0; m < moves.size(); m++) {
        const Move &mv = moves[m];
        int i = (colorInt == 0) ? toIndex(mv.start_rank, mv.start_file) : toIndexRotated(mv.start_rank, mv.start_file);
        int j = (colorInt == 0) ? toIndex(mv.end_rank,   mv.end_file)   : toIndexRotated(mv.end_rank,   mv.end_file);
        std::string prom;

        oss << i << "," << j << "," << prom;
        if (m + 1 < moves.size()) {
            oss << ";";
        }
    }

    std::string result = oss.str();
    char* ret = new char[result.size() + 1];
    std::copy(result.begin(), result.end(), ret);
    ret[result.size()] = '\0';
    return ret;
}

void free_string(const char* s)
{
    delete[] s;
}
