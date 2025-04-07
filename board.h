#ifndef BOARD_H
#define BOARD_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <unordered_set>
#include <functional> // For hash
#include <cstddef>
#include <stack>
#include "Piece.h"
#include "magic.h"

using namespace std;

const uint64_t FILE_A = 0x0101010101010101ULL; // The A file (left-most)
const uint64_t FILE_H = 0x8080808080808080ULL; // The H file (right-most)

struct Move {
    int start_file, start_rank;
    int end_file, end_rank;
};


struct BoardState {
    uint64_t white_pawns, white_knights, white_bishops, white_rooks, white_queens, white_kings;
    uint64_t black_pawns, black_knights, black_bishops, black_rooks, black_queens, black_kings;
    uint16_t castling_rights; // 4 bits to represent castling (KQkq)
    int en_passant_square; // 0-63 for a valid square, or 64 if none

    bool operator==(const BoardState &other) const {
        return white_pawns == other.white_pawns &&
               white_knights == other.white_knights &&
               white_bishops == other.white_bishops &&
               white_rooks == other.white_rooks &&
               white_queens == other.white_queens &&
               white_kings == other.white_kings &&
               black_pawns == other.black_pawns &&
               black_knights == other.black_knights &&
               black_bishops == other.black_bishops &&
               black_rooks == other.black_rooks &&
               black_queens == other.black_queens &&
               black_kings == other.black_kings &&
               castling_rights == other.castling_rights &&
               en_passant_square == other.en_passant_square;
    }
};


struct BoardStateHasher {
    std::size_t operator()(const BoardState& state) const {
        std::size_t hash = 0;

        // Combine hashes for all bitboards
        hash ^= std::hash<uint64_t>()(state.white_pawns) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.white_knights) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.white_bishops) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.white_rooks) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.white_queens) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.white_kings) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.black_pawns) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.black_knights) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.black_bishops) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.black_rooks) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.black_queens) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint64_t>()(state.black_kings) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

        // Combine hashes for castling rights and en passant square
        hash ^= std::hash<uint16_t>()(state.castling_rights) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>()(state.en_passant_square) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

        return hash;
    }
};


class Board {

    friend void loadBoard(Board &board, const char* b);
    friend void clearBoard(Board & board);
    friend int toIndex(int rank, int file);
    friend bool fromIndex(int sfIndex, int &outRank, int &outFile);

public:

    Board();

    void generate_board();  // Initialize the board using bitboards
    void display();  // Display the current board
    void print_position();
    BoardState track_position();
    void compute_attack_masks();
    void initialize_magic_bitboards();

    void make_move(const Move & move);
    void undo_move(const Move & move, const Piece & captured_piece);
    void generate_pawn_moves(uint64_t pawns, Color color, vector<Move> & moves);
    void generate_knight_moves(uint64_t knights, Color color, vector<Move> & moves);
    void generate_rook_moves(uint64_t rooks, Color color, vector<Move> & moves);
    void generate_bishop_moves(uint64_t bishops, Color color, vector<Move> & moves);
    void generate_queen_moves(uint64_t queens, Color color, vector<Move> & moves);
    void generate_king_moves(uint64_t kings, Color color, vector<Move> & moves);
    void init();
    void clear();
    
    // Bitboard-specific methods
    uint64_t get_bitboard(PieceType type, Color color) const;
    uint64_t generate_knight_attack_mask(uint64_t knight_pos);
    uint64_t generate_king_attack_mask(uint64_t king_pos);
    uint64_t generate_rook_attack_mask(uint64_t rook_pos, uint64_t all_pieces);
    uint64_t generate_bishop_attack_mask(uint64_t bishop_pos);
    uint64_t generate_queen_attack_mask(uint64_t queen_pos, uint64_t occupancy);

    uint64_t & get_white_attacks();
    uint64_t & get_black_attacks();

    bool is_check(int new_row, int new_col, Color color);
    bool is_check(Color king_color);
    bool is_check(uint64_t pos, Color king_color);
    bool is_checkmate(Color color);
    bool is_stalemate(Color color);
    bool check_castling(Piece & p, const Move & m);
    bool is_within_bounds(int rank, int file);

    uint16_t getCastlingRights();
    uint8_t getEnPassantSquare() const ;

    vector<Move> generate_all_moves(Color color);
    vector<Move> generate_legal_moves(Color color);
    vector<Move> generate_pseudo_legal_moves(Color color);

    BoardState get_current_board_state();
    Color get_color();

    size_t get_turn_index();
    void set_turn_index(size_t index);

    const Piece & getPiece(int square);
    const Piece & getPiece(int rank, int file);
    const Piece & get_piece_at(int rank, int file);
    const Piece & get_piece_at(int square);

    bool is_square_attacked(int square, Color attacker_color) const;
    vector<int> find_checking_pieces(int king_sq, Color attacker_color) const;
    int get_direction(int from_sq, int to_sq) const;
    void generate_captures(Color color, int checker_sq, vector<Move> & moves);
    void generate_blocks(Color color, int checker_sq, vector<Move> & moves);
    void generate_king_moves(uint64_t kings, Color color, vector<Move> & moves, bool double_check);
    uint64_t get_between_squares(int from_sq, int to_sq) const;
    bool is_sliding_piece(PieceType type) const;


private:
    // Bitboards for each piece type and color
    uint64_t white_pawns;
    uint64_t black_pawns;
    uint64_t white_knights;
    uint64_t black_knights;
    uint64_t white_rooks;
    uint64_t black_rooks;
    uint64_t white_bishops;
    uint64_t black_bishops;
    uint64_t white_queens;
    uint64_t black_queens;
    uint64_t white_kings;
    uint64_t black_kings;
    uint64_t all_white_pieces;
    uint64_t all_black_pieces;
    uint64_t all_pieces;
    uint64_t white_attacks;  // Squares attacked by White
    uint64_t black_attacks;  // Squares attacked by Black
    uint64_t pinned_pieces;  // Bitboard of currently moving side's pinned pieces
    

    uint64_t KNIGHT_ATTACKS[64];
    uint64_t KING_ATTACKS[64];   


    uint64_t one_square(int sq);
    int shift_square(int sq, int dr, int dc);
    void init_precomputed_attacks();

    void filter_moves_for_legality(const vector<Move> & raw_moves, Color color, vector<Move> & legal_moves);
    void detect_pins(Color us, int king_sq);
    void init_incremental_pos();

    bool is_within_bounds(int row, int col) const;
    bool is_pinned_piece(int sq) const;

    int en_passant_square;

    vector<vector<Piece>> board;
    unordered_set<BoardState, BoardStateHasher> visited_states;

    uint64_t pin_line[64];   // For each pinned piece's square, the mask of allowed moves

    stack<int> en_passant_stack;
    stack<int> promotion_stack;
    stack<bool> castling_stack;

    magic_bits::Attacks magicAttacks; // Instance for magic bitboard attacks

    size_t turn_index = 0;
};


#endif
