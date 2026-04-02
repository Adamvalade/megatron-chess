#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include "board.h" 
#include "interface.h"
#include "search.h"

namespace {

void apply_fen_placement(Board& board, const std::string& fen) {
    clearBoard(board);
    char grid[8][8];
    std::memset(grid, '.', sizeof(grid));
    int rank = 7, file = 0;
    for (size_t i = 0; i < fen.size() && fen[i] != ' '; i++) {
        char c = fen[i];
        if (c == '/') {
            rank--;
            file = 0;
            continue;
        }
        if (c >= '1' && c <= '8') {
            file += (c - '0');
            continue;
        }
        if (rank >= 0 && rank < 8 && file >= 0 && file < 8) {
            grid[rank][file] = c;
        }
        file++;
    }
    char b120[121];
    std::memset(b120, ' ', 120);
    b120[120] = '\0';
    for (int r = 0; r < 8; r++) {
        for (int f = 0; f < 8; f++) {
            int idx = 10 * (9 - r) + (1 + f);
            b120[idx] = grid[r][f];
        }
    }
    loadBoard(board, b120);
}

bool parse_uci_move(const std::string& s, Move& m) {
    if (s.size() < 4) return false;
    m.start_file = s[0] - 'a';
    m.start_rank = s[1] - '1';
    m.end_file = s[2] - 'a';
    m.end_rank = s[3] - '1';
    return true;
}

std::string move_to_uci(const Move& m) {
    std::string out;
    out += static_cast<char>('a' + m.start_file);
    out += static_cast<char>('1' + m.start_rank);
    out += static_cast<char>('a' + m.end_file);
    out += static_cast<char>('1' + m.end_rank);
    return out;
}

} // namespace



void clearBoard(Board & board) {
    board.clear();
}


int toIndex(int rank, int file) {
    return 10 * (9 - rank) + (1 + file);
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

void loadBoard(Board & board, const char* b)
{
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



const char * get_legal_moves(const char* sunfishBoard, int colorInt)
{
    Board board;
    clearBoard(board);          
    loadBoard(board, sunfishBoard); 

    Color sideToMove = (colorInt == 0) ? Color::WHITE : Color::BLACK;

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
        int i = toIndex(mv.start_rank, mv.start_file);
        int j = toIndex(mv.end_rank,   mv.end_file);
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

const char* search_position(const char* fen_pieces, int colorInt, int ep_sq,
                            const char* moves_uci, int max_depth, int max_time_ms) {
    Board board;
    std::string fen = fen_pieces ? fen_pieces : "startpos";
    if (fen == "startpos") {
        board.clear();
        board.generate_board();
    } else {
        apply_fen_placement(board, fen);
    }

    int n_moves = 0;
    if (moves_uci && moves_uci[0] != '\0') {
        std::istringstream ms(moves_uci);
        std::string tok;
        while (ms >> tok) {
            n_moves++;
        }
    }

    int init_stm = (n_moves % 2 == 0) ? colorInt : (1 - colorInt);
    Color cur = (init_stm == 0) ? Color::WHITE : Color::BLACK;

    if (moves_uci && moves_uci[0] != '\0') {
        std::istringstream ms2(moves_uci);
        std::string tok;
        while (ms2 >> tok) {
            Move m;
            if (!parse_uci_move(tok, m)) {
                continue;
            }
            board.compute_attack_masks();
            board.make_move(m);
            search_rebuild_bitboards_from_2d(board);
            cur = (cur == Color::WHITE) ? Color::BLACK : Color::WHITE;
        }
    }

    board.set_turn_index(cur == Color::WHITE ? 0 : 1);
    board.reset_en_passant_stack(ep_sq);
    search_rebuild_bitboards_from_2d(board);
    board.compute_attack_masks();

    Searcher searcher;
    int out_score = 0, out_depth = 0;
    uint64_t out_nodes = 0;
    Move best = searcher.search(board, cur,
                                max_depth > 0 ? max_depth : 100,
                                max_time_ms > 0 ? max_time_ms : 0,
                                out_score, out_depth, out_nodes);

    std::ostringstream oss;
    if (best.start_rank >= 0 && best.start_file >= 0) {
        oss << move_to_uci(best);
    } else {
        oss << "0000";
    }
    oss << " " << out_score << " " << out_depth << " " << out_nodes;

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
