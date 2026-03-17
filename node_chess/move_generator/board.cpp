#include "board.h"
#include <unordered_map>

using namespace std;

Board::Board() {
    init();
}

void Board::init() {
    generate_board();
    initialize_magic_bitboards();
    init_precomputed_attacks();
}

void Board::clear() {
    white_pawns = black_pawns = white_knights = black_knights =
    white_rooks = black_rooks = white_bishops = black_bishops =
    white_queens = black_queens = white_kings = black_kings =
    all_white_pieces = all_black_pieces = all_pieces =
    white_attacks = black_attacks = 0ULL;

    board.clear();
    board.resize(8, vector<Piece>(8, Piece(PieceType::NONE, Color::NONE)));
    turn_index = 0;
}
    

void Board::generate_board() {

    board.resize(8, vector<Piece>(8, Piece(PieceType::NONE, Color::NONE)));

    // Initialize the bitboards for each piece type
    white_pawns = 0x000000000000FF00;
    black_pawns = 0x00FF000000000000;
    white_knights = 0x0000000000000042;
    black_knights = 0x4200000000000000;
    white_rooks = 0x0000000000000081;
    black_rooks = 0x8100000000000000;
    white_bishops = 0x0000000000000024;
    black_bishops = 0x2400000000000000;
    white_queens = 0x0000000000000008;
    black_queens = 0x0800000000000000;
    white_kings = 0x0000000000000010;
    black_kings = 0x1000000000000000;

    for (size_t i = 0; i < 8; ++i) 
    {
        board[1][i] = Piece(PieceType::PAWN, Color::WHITE);
        board[6][i] = Piece(PieceType::PAWN, Color::BLACK);
    }

    board[0][0] = board[0][7] = Piece(PieceType::ROOK, Color::WHITE);
    board[0][1] = board[0][6] = Piece(PieceType::KNIGHT, Color::WHITE);
    board[0][2] = board[0][5] = Piece(PieceType::BISHOP, Color::WHITE);
    board[0][3] = Piece(PieceType::QUEEN, Color::WHITE);
    board[0][4] = Piece(PieceType::KING, Color::WHITE);

    board[7][0] = board[7][7] = Piece(PieceType::ROOK, Color::BLACK);
    board[7][1] = board[7][6] = Piece(PieceType::KNIGHT, Color::BLACK);
    board[7][2] = board[7][5] = Piece(PieceType::BISHOP, Color::BLACK);
    board[7][3] = Piece(PieceType::QUEEN, Color::BLACK);
    board[7][4] = Piece(PieceType::KING, Color::BLACK);
    
    all_white_pieces = white_pawns | white_knights | white_rooks | white_bishops | white_queens | white_kings;
    all_black_pieces = black_pawns | black_knights | black_rooks | black_bishops | black_queens | black_kings;
    all_pieces = all_white_pieces | all_black_pieces;
}


// Helper function to get a bitboard for a specific piece type and color
uint64_t Board::get_bitboard(PieceType type, Color color) const {
    switch (type) {
        case PieceType::PAWN:
            return (color == Color::WHITE) ? white_pawns : black_pawns;
        case PieceType::KNIGHT:
            return (color == Color::WHITE) ? white_knights : black_knights;
        case PieceType::ROOK:
            return (color == Color::WHITE) ? white_rooks : black_rooks;
        case PieceType::BISHOP:
            return (color == Color::WHITE) ? white_bishops : black_bishops;
        case PieceType::QUEEN:
            return (color == Color::WHITE) ? white_queens : black_queens;
        case PieceType::KING:
            return (color == Color::WHITE) ? white_kings : black_kings;
        default:
            return 0;
    }
}

// Helper function to check if a square is within bounds
bool Board::is_within_bounds(int row, int col) const {
    return (row >= 0 && row < 8 && col >= 0 && col < 8);
}


// Piece lookup for display purposes
const Piece & Board::getPiece(int rank, int file) {
    return board[static_cast<size_t>(rank)][static_cast<size_t>(file)];
}

const Piece & Board::getPiece(int square) {
    int rank = square / 8;
    int file = square % 8;
    return board[static_cast<size_t>(rank)][static_cast<size_t>(file)];
}

const Piece & Board::get_piece_at(int rank, int file) {
    return board[static_cast<size_t>(rank)][static_cast<size_t>(file)];
}

const Piece & Board::get_piece_at(int square) {
    int rank = square / 8;
    int file = square % 8;
    return board[static_cast<size_t>(rank)][static_cast<size_t>(file)];
}

void Board::initialize_magic_bitboards() 
{
//    initMagicTables();
    return;
}

// Apply a move to the board (bitboard and 2D array)
void Board::make_move(const Move & move) {
    // 1) Validate from/to squares, etc.
    if (move.start_rank < 0 || move.start_rank >= 8 || 
        move.start_file < 0 || move.start_file >= 8 ||
        move.end_rank < 0 || move.end_rank >= 8 ||
        move.end_file < 0 || move.end_file >= 8) 
    {
        cout << "invalid move\n";
        assert(false);
    }

    // 2) Fetch the piece
    Piece piece = board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)];
    Piece captured_piece = board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)];

    bool castled = check_castling(piece, move);

    if (castled)
        return;

    castling_stack.push(false);  

    // Update the 2D array
    board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)] = Piece(PieceType::NONE, Color::NONE);
    board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)] = piece;

    // Update bitboards
    uint64_t start_bit = 1ULL << (move.start_rank * 8 + move.start_file);
    uint64_t end_bit = 1ULL << (move.end_rank * 8 + move.end_file);
    

    Color c = piece.getColor();

    if (!en_passant_stack.empty()) {
        int en = en_passant_stack.top();
        if ((en != 64) && piece.getType() == PieceType::PAWN && 
            ((move.end_rank * 8 + move.end_file) == en)) {

            // Determine the rank and file of the captured pawn
            int capture_rank = (c == Color::WHITE) ? move.end_rank - 1 : move.end_rank + 1;
            int capture_file = move.end_file;

            // Remove the captured pawn from the board
            board[static_cast<size_t>(capture_rank)][static_cast<size_t>(capture_file)] = Piece(PieceType::NONE, Color::NONE);

            // Update bitboards for the captured pawn
            uint64_t capture_bit = 1ULL << (capture_rank * 8 + capture_file);
            if (c == Color::WHITE) {
                black_pawns &= ~capture_bit;  // Remove black pawn
            } else {
                white_pawns &= ~capture_bit;  // Remove white pawn
            }
        }
    }

    if (piece.getType() == PieceType::PAWN && 
        ((c == Color::WHITE && move.start_rank == 1 && move.end_rank == 3) || 
        (c == Color::BLACK && move.start_rank == 6 && move.end_rank == 4))) {

        if (c == Color::WHITE) 
            en_passant_square = move.end_rank * 8 + move.end_file - 8;
        
        else 
            en_passant_square = move.end_rank * 8 + move.end_file + 8;
    } 
    else 
        en_passant_square = 64;  // Reset if no valid en passant
    
    en_passant_stack.push(en_passant_square);
        

    // Remove the piece from its old position in the bitboard
    switch (piece.getType()) {
        case PieceType::PAWN:
            piece.getColor() == Color::WHITE ? white_pawns &= ~start_bit : black_pawns &= ~start_bit;
            break;
        case PieceType::KNIGHT:
            piece.getColor() == Color::WHITE ? white_knights &= ~start_bit : black_knights &= ~start_bit;
            break;
        case PieceType::ROOK:
            piece.getColor() == Color::WHITE ? white_rooks &= ~start_bit : black_rooks &= ~start_bit;
            break;
        case PieceType::BISHOP:
            piece.getColor() == Color::WHITE ? white_bishops &= ~start_bit : black_bishops &= ~start_bit;
            break;
        case PieceType::QUEEN:
            piece.getColor() == Color::WHITE ? white_queens &= ~start_bit : black_queens &= ~start_bit;
            break;
        case PieceType::KING:
            piece.getColor() == Color::WHITE ? white_kings &= ~start_bit : black_kings &= ~start_bit;
            break;
        default:
            break;
    }

    // Add the piece to its new position in the bitboard
    switch (piece.getType()) {
        case PieceType::PAWN:
            piece.getColor() == Color::WHITE ? white_pawns |= end_bit : black_pawns |= end_bit;
            break;
        case PieceType::KNIGHT:
            piece.getColor() == Color::WHITE ? white_knights |= end_bit : black_knights |= end_bit;
            break;
        case PieceType::ROOK:
            piece.getColor() == Color::WHITE ? white_rooks |= end_bit : black_rooks |= end_bit;
            break;
        case PieceType::BISHOP:
            piece.getColor() == Color::WHITE ? white_bishops |= end_bit : black_bishops |= end_bit;
            break;
        case PieceType::QUEEN:
            piece.getColor() == Color::WHITE ? white_queens |= end_bit : black_queens |= end_bit;
            break;
        case PieceType::KING:
            piece.getColor() == Color::WHITE ? white_kings |= end_bit : black_kings |= end_bit;
            break;
        default:
            break;
    }

    // If there's a captured piece, remove it from the bitboard
    if (captured_piece.getType() != PieceType::NONE) {
        switch (captured_piece.getType()) {
            case PieceType::PAWN:
                captured_piece.getColor() == Color::WHITE ? white_pawns &= ~end_bit : black_pawns &= ~end_bit;
                break;
            case PieceType::KNIGHT:
                captured_piece.getColor() == Color::WHITE ? white_knights &= ~end_bit : black_knights &= ~end_bit;
                break;
            case PieceType::ROOK:
                captured_piece.getColor() == Color::WHITE ? white_rooks &= ~end_bit : black_rooks &= ~end_bit;
                break;
            case PieceType::BISHOP:
                captured_piece.getColor() == Color::WHITE ? white_bishops &= ~end_bit : black_bishops &= ~end_bit;
                break;
            case PieceType::QUEEN:
                captured_piece.getColor() == Color::WHITE ? white_queens &= ~end_bit : black_queens &= ~end_bit;
                break;
            case PieceType::KING:
                captured_piece.getColor() == Color::WHITE ? white_kings &= ~end_bit : black_kings &= ~end_bit;
                break;
            default:
                break;
        }
    }



    if (piece.getType() == PieceType::PAWN)  {
        int endSquareIndex = move.end_rank * 8 + move.end_file;
        // White promotion: end_rank == 7
        if (c == Color::WHITE && move.end_rank == 7) {
            // Update 2D board
            board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)] = Piece(PieceType::QUEEN, Color::WHITE);

            // Update bitboards
            white_pawns &= ~end_bit;  // remove pawn from end_bit
            white_queens |= end_bit;  // add queen to end_bit

            // Push the promotion square onto the stack
            promotion_stack.push(endSquareIndex);

        }
        // Black promotion: end_rank == 0
        else if (c == Color::BLACK && move.end_rank == 0) {
            // Update 2D board
            board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)] = Piece(PieceType::QUEEN, Color::BLACK);

            // Update bitboards
            black_pawns &= ~end_bit;  
            black_queens |= end_bit;  

            // Push the promotion square
            promotion_stack.push(endSquareIndex);
        }
        else {
            // No promotion
            promotion_stack.push(64);
        }
    }
    else {
        // The moved piece was not a pawn => definitely no promotion
        promotion_stack.push(64);
    }

    // Update overall bitboards
    turn_index++;
    all_white_pieces = white_pawns | white_knights | white_rooks | white_bishops | white_queens | white_kings;
    all_black_pieces = black_pawns | black_knights | black_rooks | black_bishops | black_queens | black_kings;
    all_pieces = all_white_pieces | all_black_pieces;
}


bool Board::check_castling(Piece & p, const Move & m) {
    // White king: e1->g1 or e1->c1
    if (p.getType() == PieceType::KING && p.getColor() == Color::WHITE && 
        m.start_rank == 0 && m.start_file == 4) {
        // White kingside
        if (m.end_rank == 0 && m.end_file == 6) {
            // Move the king
            board[0][4] = Piece(PieceType::NONE, Color::NONE);
            board[0][6] = Piece(PieceType::KING, Color::WHITE);

            // Move the rook from h1 -> f1
            board[0][7] = Piece(PieceType::NONE, Color::NONE);
            board[0][5] = Piece(PieceType::ROOK, Color::WHITE);

            // Update bitboards accordingly
            uint64_t king_start_bit = 1ULL << (0 * 8 + 4);
            white_kings &= ~king_start_bit;
            uint64_t rook_start_bit = 1ULL << (0 * 8 + 7);
            white_rooks &= ~rook_start_bit;

            uint64_t king_end_bit = 1ULL << (0 * 8 + 6);
            white_kings |= king_end_bit;
            uint64_t rook_end_bit = 1ULL << (0 * 8 + 5);
            white_rooks |= rook_end_bit;

            // Recompute aggregates
            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            // Turn done
            turn_index++;
            castling_stack.push(true);  // push "true" for castling
            return true;
        }

        // White queenside
        if (m.end_rank == 0 && m.end_file == 2) {
            // Move the king
            board[0][4] = Piece(PieceType::NONE, Color::NONE);
            board[0][2] = Piece(PieceType::KING, Color::WHITE);

            // Move the rook from a1 -> d1
            board[0][0] = Piece(PieceType::NONE, Color::NONE);
            board[0][3] = Piece(PieceType::ROOK, Color::WHITE);

            // Update bitboards
            uint64_t king_start_bit = 1ULL << (0 * 8 + 4);
            white_kings &= ~king_start_bit;
            uint64_t rook_start_bit = 1ULL << (0 * 8 + 0);
            white_rooks &= ~rook_start_bit;

            uint64_t king_end_bit = 1ULL << (0 * 8 + 2);
            white_kings |= king_end_bit;
            uint64_t rook_end_bit = 1ULL << (0 * 8 + 3);
            white_rooks |= rook_end_bit;

            // Recompute aggregates
            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            turn_index++;
            castling_stack.push(true);
            return true;
        }
    }

    // Black king: e8->g8 or e8->c8
    if (p.getType() == PieceType::KING && p.getColor() == Color::BLACK && 
        m.start_rank == 7 && m.start_file == 4) {
        // Black kingside
        if (m.end_rank == 7 && m.end_file == 6)
        {
            // Move the king
            board[7][4] = Piece(PieceType::NONE, Color::NONE);
            board[7][6] = Piece(PieceType::KING, Color::BLACK);

            // Move the rook from h8 -> f8
            board[7][7] = Piece(PieceType::NONE, Color::NONE);
            board[7][5] = Piece(PieceType::ROOK, Color::BLACK);

            // Update bitboards
            uint64_t king_start_bit = 1ULL << (7 * 8 + 4);
            black_kings &= ~king_start_bit;
            uint64_t rook_start_bit = 1ULL << (7 * 8 + 7);
            black_rooks &= ~rook_start_bit;

            uint64_t king_end_bit = 1ULL << (7 * 8 + 6);
            black_kings |= king_end_bit;
            uint64_t rook_end_bit = 1ULL << (7 * 8 + 5);
            black_rooks |= rook_end_bit;

            // Recompute aggregates
            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            turn_index++;
            castling_stack.push(true);
            return true;
        }

        // Black queenside
        if (m.end_rank == 7 && m.end_file == 2)
        {
            // Move the king
            board[7][4] = Piece(PieceType::NONE, Color::NONE);
            board[7][2] = Piece(PieceType::KING, Color::BLACK);

            // Move the rook from a8 -> d8
            board[7][0] = Piece(PieceType::NONE, Color::NONE);
            board[7][3] = Piece(PieceType::ROOK, Color::BLACK);

            // Update bitboards
            uint64_t king_start_bit = 1ULL << (7 * 8 + 4);
            black_kings &= ~king_start_bit;
            uint64_t rook_start_bit = 1ULL << (7 * 8 + 0);
            black_rooks &= ~rook_start_bit;

            uint64_t king_end_bit = 1ULL << (7 * 8 + 2);
            black_kings |= king_end_bit;
            uint64_t rook_end_bit = 1ULL << (7 * 8 + 3);
            black_rooks |= rook_end_bit;

            // Recompute aggregates
            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            turn_index++;
            castling_stack.push(true);
            return true;
        }
    }

    //castling move not made
    return false;
}


// Undo a move
void Board::undo_move(const Move & move, const Piece & captured_piece) {
    // Retrieve the piece being moved

    // Pop from the castling stack
    bool was_castling = castling_stack.top();
    castling_stack.pop();

    // If it was a castling move, revert king and rook:
    if (was_castling) {

        // White kingside? e1->g1
        if (move.start_rank == 0 && move.start_file == 4 &&
            move.end_rank == 0 && move.end_file == 6) 
        {
            // Move king back
            board[0][4] = Piece(PieceType::KING, Color::WHITE);
            board[0][6] = Piece(PieceType::NONE, Color::NONE);

            // Move rook back
            board[0][7] = Piece(PieceType::ROOK, Color::WHITE);
            board[0][5] = Piece(PieceType::NONE, Color::NONE);

            // Bitboards
            uint64_t king_end_bit = 1ULL << (0 * 8 + 6);
            white_kings &= ~king_end_bit;
            uint64_t rook_end_bit = 1ULL << (0 * 8 + 5);
            white_rooks &= ~rook_end_bit;

            uint64_t king_start_bit = 1ULL << (0 * 8 + 4);
            white_kings |= king_start_bit;
            uint64_t rook_start_bit = 1ULL << (0 * 8 + 7);
            white_rooks |= rook_start_bit;

            // Recompute aggregates
            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;
            
            turn_index--;
            return;
        }
        // White queenside? e1->c1
        if (move.start_rank == 0 && move.start_file == 4 &&
            move.end_rank == 0 && move.end_file == 2) 
        {
            // Revert king
            board[0][4] = Piece(PieceType::KING, Color::WHITE);
            board[0][2] = Piece(PieceType::NONE, Color::NONE);

            // Revert rook
            board[0][0] = Piece(PieceType::ROOK, Color::WHITE);
            board[0][3] = Piece(PieceType::NONE, Color::NONE);

            // Bitboards
            uint64_t king_end_bit = 1ULL << (0 * 8 + 2);
            white_kings &= ~king_end_bit;
            uint64_t rook_end_bit = 1ULL << (0 * 8 + 3);
            white_rooks &= ~rook_end_bit;

            uint64_t king_start_bit = 1ULL << (0 * 8 + 4);
            white_kings |= king_start_bit;
            uint64_t rook_start_bit = 1ULL << (0 * 8 + 0);
            white_rooks |= rook_start_bit;

            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            turn_index--;
            return;
        }
        // Black kingside? e8->g8
        if (move.start_rank == 7 && move.start_file == 4 &&
            move.end_rank == 7 && move.end_file == 6) 
        {
            // Revert king
            board[7][4] = Piece(PieceType::KING, Color::BLACK);
            board[7][6] = Piece(PieceType::NONE, Color::NONE);

            // Revert rook
            board[7][7] = Piece(PieceType::ROOK, Color::BLACK);
            board[7][5] = Piece(PieceType::NONE, Color::NONE);

            // Bitboards
            uint64_t king_end_bit = 1ULL << (7 * 8 + 6);
            black_kings &= ~king_end_bit;
            uint64_t rook_end_bit = 1ULL << (7 * 8 + 5);
            black_rooks &= ~rook_end_bit;

            uint64_t king_start_bit = 1ULL << (7 * 8 + 4);
            black_kings |= king_start_bit;
            uint64_t rook_start_bit = 1ULL << (7 * 8 + 7);
            black_rooks |= rook_start_bit;

            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            turn_index--;
            return;
        }
        // Black queenside? e8->c8
        if (move.start_rank == 7 && move.start_file == 4 &&
            move.end_rank == 7 && move.end_file == 2) 
        {
            // Revert king
            board[7][4] = Piece(PieceType::KING, Color::BLACK);
            board[7][2] = Piece(PieceType::NONE, Color::NONE);

            // Revert rook
            board[7][0] = Piece(PieceType::ROOK, Color::BLACK);
            board[7][3] = Piece(PieceType::NONE, Color::NONE);

            // Bitboards
            uint64_t king_end_bit = 1ULL << (7 * 8 + 2);
            black_kings &= ~king_end_bit;
            uint64_t rook_end_bit = 1ULL << (7 * 8 + 3);
            black_rooks &= ~rook_end_bit;

            uint64_t king_start_bit = 1ULL << (7 * 8 + 4);
            black_kings |= king_start_bit;
            uint64_t rook_start_bit = 1ULL << (7 * 8 + 0);
            black_rooks |= rook_start_bit;

            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            turn_index--;
            return;
        }
    }
    // not castling \/\/\/\/\/\/

    Piece piece = board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)];

    // Update the 2D array
    board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)] = piece;
    board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)] = captured_piece;

    // Bit manipulation
    uint64_t start_bit = 1ULL << (move.start_rank * 8 + move.start_file);
    uint64_t end_bit = 1ULL << (move.end_rank * 8 + move.end_file);

    // Remove the moved piece from the destination square
    switch (piece.getType()) {
        case PieceType::PAWN:
            piece.getColor() == Color::WHITE ? white_pawns &= ~end_bit : black_pawns &= ~end_bit;
            break;
        case PieceType::KNIGHT:
            piece.getColor() == Color::WHITE ? white_knights &= ~end_bit : black_knights &= ~end_bit;
            break;
        case PieceType::ROOK:
            piece.getColor() == Color::WHITE ? white_rooks &= ~end_bit : black_rooks &= ~end_bit;
            break;
        case PieceType::BISHOP:
            piece.getColor() == Color::WHITE ? white_bishops &= ~end_bit : black_bishops &= ~end_bit;
            break; 
        case PieceType::QUEEN:
            piece.getColor() == Color::WHITE ? white_queens &= ~end_bit : black_queens &= ~end_bit;
            break;
        case PieceType::KING:
            piece.getColor() == Color::WHITE ? white_kings &= ~end_bit : black_kings &= ~end_bit;
            break;
        default:
            break;
    }

    // Restore the moved piece to the starting square
    switch (piece.getType()) {
        case PieceType::PAWN:
            piece.getColor() == Color::WHITE ? white_pawns |= start_bit : black_pawns |= start_bit;
            break;
        case PieceType::KNIGHT:
            piece.getColor() == Color::WHITE ? white_knights |= start_bit : black_knights |= start_bit;
            break;
        case PieceType::ROOK:
            piece.getColor() == Color::WHITE ? white_rooks |= start_bit : black_rooks |= start_bit;
            break;
        case PieceType::BISHOP:
            piece.getColor() == Color::WHITE ? white_bishops |= start_bit : black_bishops |= start_bit;
            break;
        case PieceType::QUEEN:
            piece.getColor() == Color::WHITE ? white_queens |= start_bit : black_queens |= start_bit;
            break;
        case PieceType::KING:
            piece.getColor() == Color::WHITE ? white_kings |= start_bit : black_kings |= start_bit;
            break;
        default:
            break;
    }

    // Restore the captured piece if it exists
    if (captured_piece.getType() != PieceType::NONE) {
        switch (captured_piece.getType()) {
            case PieceType::PAWN:
                captured_piece.getColor() == Color::WHITE ? white_pawns |= end_bit : black_pawns |= end_bit;
                break;
            case PieceType::KNIGHT:
                captured_piece.getColor() == Color::WHITE ? white_knights |= end_bit : black_knights |= end_bit;
                break;
            case PieceType::ROOK:
                captured_piece.getColor() == Color::WHITE ? white_rooks |= end_bit : black_rooks |= end_bit;
                break;
            case PieceType::BISHOP:
                captured_piece.getColor() == Color::WHITE ? white_bishops |= end_bit : black_bishops |= end_bit;
                break;
            case PieceType::QUEEN:
                captured_piece.getColor() == Color::WHITE ? white_queens |= end_bit : black_queens |= end_bit;
                break;
            case PieceType::KING:
                captured_piece.getColor() == Color::WHITE ? white_kings |= end_bit : black_kings |= end_bit;
                break;
            default:
                break;
        }
    }


    int promoted_square = promotion_stack.top();
    promotion_stack.pop();

    if (promoted_square != 64) {
        // => That means a pawn was promoted at 'promoted_square'.
        // We need to revert it to a pawn. The 'moved_piece' is currently the
        // post-promotion piece (e.g. a queen, if you always promote to queen).

        // If you do always promote to queen, then:
        Color promoColor = piece.getColor();

        // - Remove the queen from the start_bit
        if (promoColor == Color::WHITE) {
            white_queens &= ~start_bit; 
        } else {
            black_queens &= ~start_bit;
        }

        // - Add the pawn back
        if (promoColor == Color::WHITE) {
            white_pawns |= start_bit;
        } else {
            black_pawns |= start_bit;
        }

        // 2) Also revert the 2D board array. 
        board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)] = Piece(PieceType::PAWN, promoColor);
    }

    en_passant_stack.pop();
    int ep;
    if (!en_passant_stack.empty())
        ep = en_passant_stack.top();
    else
        ep = 64;

    // Restore the board state for en passant capture, if applicable
    if (move.end_rank * 8 + move.end_file == ep &&
        board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)].getType() == PieceType::PAWN) {

        // Determine where the captured pawn should be restored
        int capture_rank = (board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)].getColor() == Color::WHITE)
                            ? move.end_rank - 1
                            : move.end_rank + 1;
        int capture_file = move.end_file;

        // Restore the captured pawn
        board[static_cast<size_t>(capture_rank)][static_cast<size_t>(capture_file)] = Piece(PieceType::PAWN, 
            board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)].getColor() == Color::WHITE ? Color::BLACK : Color::WHITE);

        uint64_t capture_bit = 1ULL << (capture_rank * 8 + capture_file);
        if (board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)].getColor() == Color::WHITE) {
            black_pawns |= capture_bit;
        } else {
            white_pawns |= capture_bit;
        }
    }

    // Update overall bitboards
    turn_index--;
    all_white_pieces = white_pawns | white_knights | white_rooks | white_bishops | white_queens | white_kings;
    all_black_pieces = black_pawns | black_knights | black_rooks | black_bishops | black_queens | black_kings;
    all_pieces = all_white_pieces | all_black_pieces;
}


void Board::display() {
    // Display column labels
    cout << "  a b c d e f g h\n";

    for (int rank = 7; rank >= 0; --rank) {  
        cout << rank + 1 << " "; 

        for (int file = 0; file < 8; ++file) {
            const Piece & piece = getPiece(rank, file);

            switch (piece.getType()) {
                case PieceType::PAWN:
                    cout << ((piece.getColor() == Color::WHITE) ? "P" : "p") << " ";
                    break;
                case PieceType::KNIGHT:
                    cout << ((piece.getColor() == Color::WHITE) ? "N" : "n") << " ";
                    break;
                case PieceType::ROOK:
                    cout << ((piece.getColor() == Color::WHITE) ? "R" : "r") << " ";
                    break;
                case PieceType::BISHOP:
                    cout << ((piece.getColor() == Color::WHITE) ? "B" : "b") << " ";
                    break;
                case PieceType::QUEEN:
                    cout << ((piece.getColor() == Color::WHITE) ? "Q" : "q") << " ";
                    break;
                case PieceType::KING:
                    cout << ((piece.getColor() == Color::WHITE) ? "K" : "k") << " ";
                    break;
                default:
                    cout << ". ";  // Empty square
                    break;
            }
        }
        cout << " " << rank + 1 << "\n";  
    }
    cout << "  a b c d e f g h\n\n\n";  
}



vector<Move> Board::generate_all_moves(Color color) {
    return generate_legal_moves(color);
}




void Board::generate_pawn_moves(uint64_t pawns, Color color, vector<Move> & moves) {
    // Precompute empty squares, direction, etc.
    uint64_t empty = ~all_pieces;  
    int direction = (color == Color::WHITE) ? 8 : -8;

    // Single pushes
    uint64_t single_pushes = (color == Color::WHITE)
                                ? ((pawns << 8) & empty)
                                : ((pawns >> 8) & empty);

    while (single_pushes) {
        int to_square = __builtin_ctzll(single_pushes);
        single_pushes &= (single_pushes - 1);

        int from_square = to_square - direction;
        moves.push_back({from_square % 8, from_square / 8,
                        to_square % 8,   to_square / 8});
    }

    // Double pushes (only from starting rank)
    uint64_t start_rank = (color == Color::WHITE)
                            ? (pawns & 0x000000000000FF00ULL) // rank 2
                            : (pawns & 0x00FF000000000000ULL); // rank 7
    uint64_t double_pushes = (color == Color::WHITE)
                                ? ((start_rank << 16) & empty & (empty << 8))
                                : ((start_rank >> 16) & empty & (empty >> 8));

    while (double_pushes) {
        int to_square = __builtin_ctzll(double_pushes);
        double_pushes &= (double_pushes - 1);

        int from_square = to_square - 2*direction;
        moves.push_back({from_square % 8, from_square / 8,
                        to_square % 8,   to_square / 8});
    }

    // Captures
    constexpr uint64_t FILE_H = 0x0101010101010101ULL; 
    constexpr uint64_t FILE_A = 0x8080808080808080ULL;

    // left captures
    uint64_t left_capt = (color == Color::WHITE)
                            ? ((pawns << 9) & ~FILE_H & all_black_pieces)
                            : ((pawns >> 7) & ~FILE_H & all_white_pieces);

    while (left_capt) {
        int to_square = __builtin_ctzll(left_capt);
        left_capt &= (left_capt - 1);

        int from_square = to_square - (direction + 1);
        moves.push_back({from_square % 8, from_square / 8,
                        to_square % 8,   to_square / 8});
    }

    // right captures
    uint64_t right_capt = (color == Color::WHITE)
                            ? ((pawns << 7) & ~FILE_A & all_black_pieces)
                            : ((pawns >> 9) & ~FILE_A & all_white_pieces);

    while (right_capt) {
        int to_square = __builtin_ctzll(right_capt);
        right_capt &= (right_capt - 1);

        int from_square = to_square - (direction - 1);
        moves.push_back({from_square % 8, from_square / 8,
                        to_square % 8,   to_square / 8});
    }

    // En passant
    int ep = (!en_passant_stack.empty()) ? en_passant_stack.top() : 64;
    if (ep != 64) {
        uint64_t ep_bit = 1ULL << ep;

        uint64_t ep_left = (color == Color::WHITE)
                            ? ((pawns << 9) & ~FILE_H & ep_bit)
                            : ((pawns >> 7) & ~FILE_H & ep_bit);
        if (ep_left) {
            int to_square = __builtin_ctzll(ep_left);
            int from_square = to_square - (direction + 1);
            moves.push_back({from_square % 8, from_square / 8,
                            to_square % 8,   to_square / 8});
        }

        uint64_t ep_right = (color == Color::WHITE)
                            ? ((pawns << 7) & ~FILE_A & ep_bit)
                            : ((pawns >> 9) & ~FILE_A & ep_bit);
        if (ep_right) {
            int to_square = __builtin_ctzll(ep_right);
            int from_square = to_square - (direction - 1);
            moves.push_back({from_square % 8, from_square / 8,
                            to_square % 8,   to_square / 8});
        }
    }
}





void Board::generate_knight_moves(uint64_t knights, Color color, std::vector<Move> & moves) 
{
    // Remove generate_knight_attack_mask call; use KNIGHT_ATTACKS instead.

    while (knights) {
        int from_square = __builtin_ctzll(knights); 
        knights &= knights - 1; // pop the knight bit

        // Precomputed attacks for this square
        uint64_t attack_mask = KNIGHT_ATTACKS[from_square];

        // Exclude friendly pieces
        uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;
        uint64_t valid_moves = attack_mask & ~friendlies;

        // Convert set bits into moves
        while (valid_moves) {
            int to_square = __builtin_ctzll(valid_moves);
            valid_moves &= (valid_moves - 1);

            // push the move
            moves.push_back(Move{
                from_square % 8, from_square / 8,
                to_square % 8,   to_square / 8
            });
        }
    }
}




void Board::generate_rook_moves(uint64_t rooks, Color color, std::vector<Move> & moves) {
    while (rooks) {
        int from_sq = __builtin_ctzll(rooks);
        uint64_t from_bit = (1ULL << from_sq);

        // Use magic
        uint64_t attack_mask = generate_rook_attack_mask(from_bit, all_pieces);

        // Filter out same-color occupancy
        uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;
        uint64_t valid_moves = attack_mask & ~friendlies;

        // Convert each set bit in valid_moves to a move
        while (valid_moves) {
            int to_sq = __builtin_ctzll(valid_moves);
            moves.push_back(Move{
                from_sq % 8, from_sq / 8,
                to_sq % 8,   to_sq / 8
            });
            valid_moves &= (valid_moves - 1);
        }
        rooks &= (rooks - 1);
    }
}


void Board::generate_bishop_moves(uint64_t bishops, Color color, std::vector<Move> & moves) 
{
    while (bishops) {
        // Identify the square of one bishop.
        int from_sq = __builtin_ctzll(bishops);   // index [0..63]

        // Occupancy is all pieces (both white and black).
        uint64_t occupancy = all_white_pieces | all_black_pieces;

        // Get squares occupied by friendly pieces (which we cannot land on).
        uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;

        // Use magic bitboard for bishop attacks.
        uint64_t attack_mask = magicAttacks.Bishop(occupancy, from_sq);

        // Exclude any friendly pieces so we don’t capture ourselves.
        uint64_t valid_moves = attack_mask & ~friendlies;

        // Convert each set bit in valid_moves to a Move object.
        while (valid_moves) {
            int to_sq = __builtin_ctzll(valid_moves);  // first (lowest) set bit
            moves.push_back(Move{
                /*start_file*/ from_sq % 8, 
                /*start_rank*/ from_sq / 8,
                /*end_file*/   to_sq % 8, 
                /*end_rank*/   to_sq / 8
            });
            valid_moves &= (valid_moves - 1);  // Clear the least significant 1-bit
        }

        // Remove this bishop’s bit from the set, move on to the next bishop.
        bishops &= (bishops - 1);
    }
}


void Board::generate_queen_moves(uint64_t queens, Color color, std::vector<Move> & moves) 
{
    while (queens) {
        // Identify the square of one queen.
        int from_sq = __builtin_ctzll(queens);


        uint64_t occupancy  = all_white_pieces | all_black_pieces;
        uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;

        // 1) Using the built-in magic Bits Queen() method:
        uint64_t attack_mask = magicAttacks.Queen(occupancy, from_sq);

        // 2) Alternatively, compute bishop + rook attacks:
        // uint64_t attack_mask = s_magicAttacks.Bishop(occupancy, from_sq) 
        //                      | s_magicAttacks.Rook(occupancy, from_sq);

        uint64_t valid_moves = attack_mask & ~friendlies;

        // Turn each set bit into a legal move
        while (valid_moves) {
            int to_sq = __builtin_ctzll(valid_moves);
            moves.push_back(Move{
                /*start_file*/ from_sq % 8,
                /*start_rank*/ from_sq / 8,
                /*end_file*/   to_sq % 8,
                /*end_rank*/   to_sq / 8
            });
            valid_moves &= (valid_moves - 1);
        }

        queens &= (queens - 1);
    }
}


uint64_t Board::one_square(int sq) {
    return 1ULL << sq;
}

// Offsets a square by (dr, dc) if still on board
int Board::shift_square(int sq, int dr, int dc)
{
    int r = sq / 8;
    int c = sq % 8;
    int nr = r + dr;
    int nc = c + dc;
    if (nr < 0 || nr > 7 || nc < 0 || nc > 7) {
        return -1; // off-board
    }
    return nr * 8 + nc;
}

void Board::init_precomputed_attacks()
{
    // For each square, compute knight & king moves
    for (int sq = 0; sq < 64; sq++) {
        KNIGHT_ATTACKS[sq] = 0ULL;
        KING_ATTACKS[sq] = 0ULL;

        // Knight possible offsets
        static const int knightOffsets[8][2] = {
            { 2,  1}, { 2, -1}, {-2,  1}, {-2, -1},
            { 1,  2}, { 1, -2}, {-1,  2}, {-1, -2}
        };

        // King possible offsets
        static const int kingOffsets[8][2] = {
            { 1, 0}, {-1, 0}, {0,  1}, {0, -1},
            { 1, 1}, { 1, -1}, {-1, 1}, {-1, -1}
        };

        // Build knight attacks
        for (auto & off : knightOffsets) {
            int target = shift_square(sq, off[0], off[1]);
            if (target != -1) {
                KNIGHT_ATTACKS[sq] |= one_square(target);
            }
        }

        // Build king attacks
        for (auto & off : kingOffsets) {
            int target = shift_square(sq, off[0], off[1]);
            if (target != -1) {
                KING_ATTACKS[sq] |= one_square(target);
            }
        }
    }
}



void Board::generate_king_moves(uint64_t kings, Color color, vector<Move> & moves) {

if (kings == 0ULL) return;


    int from_square = __builtin_ctzll(kings); // presumably only one king bit
    uint64_t attack_mask = KING_ATTACKS[from_square];
    
    uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;
    uint64_t valid_moves = attack_mask & ~friendlies;

    // Each set bit in valid_moves is a potential destination
    while (valid_moves) {
        int to_square = __builtin_ctzll(valid_moves);
        valid_moves &= valid_moves - 1;

        moves.push_back(Move{
            from_square % 8, from_square / 8,
            to_square % 8,   to_square / 8
        });
    }

    uint16_t rights = getCastlingRights();
    // king can't castle through check
    bool king_in_check = (color == Color::WHITE) ? is_check(Color::WHITE) : is_check(Color::BLACK);
    if (king_in_check) {
        // If king is in check, no castling possible
        return;
    }

    if (color == Color::WHITE) {
    
        if (rights & 0b0001)  // White kingside
        {
            // Are f1 (5) and g1 (6) both free?
            bool f1_empty = !(all_pieces & (1ULL << 5));
            bool g1_empty = !(all_pieces & (1ULL << 6));
            // Are f1, g1 not attacked by black?
            bool f1_safe = !(black_attacks & (1ULL << 5));
            bool g1_safe = !(black_attacks & (1ULL << 6));

            if (f1_empty && g1_empty && f1_safe && g1_safe) {
                // We can add a castling move e1->g1 as a single move
                // The move of the rook will be handled in make_move or a special castling code.
                moves.push_back(Move{
                    4 % 8, 4 / 8,   // from e1
                    6 % 8, 6 / 8    // to   g1
                });
            }
        }

        // Check White Queen-side castling (e1 -> c1), rook on a1 => squares b1, c1, d1 => indexes (1,2,3)
        if (rights & 0b0010)  // White can castle queenside
        {
            // Are b1 (1), c1 (2), d1 (3) all free except maybe we allow the rook to be on a1?
            bool b1_empty = !(all_pieces & (1ULL << 1));
            bool c1_empty = !(all_pieces & (1ULL << 2));
            bool d1_empty = !(all_pieces & (1ULL << 3));
            // Check squares c1, d1 are not attacked (king must not pass through or land on attacked squares)
            bool c1_safe = !(black_attacks & (1ULL << 2));
            bool d1_safe = !(black_attacks & (1ULL << 3));

            if (b1_empty && c1_empty && d1_empty && c1_safe && d1_safe) {
                moves.push_back(Move{
                    4 % 8, 4 / 8,   // from e1
                    2 % 8, 2 / 8    // to   c1
                });
            }
        }
    }

    else // color == Color::BLACK
    {
        // Black King is on e8 => from_square == 60
        // Check Black King-side castling (e8 -> g8), rook on h8 => squares f8,g8 => indexes (61,62)
        if (rights & 0b0100)  // Black can castle kingside
        {
            bool f8_empty = !(all_pieces & (1ULL << 61));
            bool g8_empty = !(all_pieces & (1ULL << 62));
            // Are f8,g8 not attacked by white?
            bool f8_safe = !(white_attacks & (1ULL << 61));
            bool g8_safe = !(white_attacks & (1ULL << 62));

            if (f8_empty && g8_empty && f8_safe && g8_safe) {
                moves.push_back(Move{
                    60 % 8, 60 / 8,  // from e8
                    62 % 8, 62 / 8   // to   g8
                });
            }
        }

        // Check Black Queen-side castling (e8 -> c8), rook on a8 => squares b8, c8, d8 => indexes (57,58,59)
        if (rights & 0b1000)  // Black can castle queenside
        {
            bool b8_empty = !(all_pieces & (1ULL << 57));
            bool c8_empty = !(all_pieces & (1ULL << 58));
            bool d8_empty = !(all_pieces & (1ULL << 59));
            bool c8_safe = !(white_attacks & (1ULL << 58));
            bool d8_safe = !(white_attacks & (1ULL << 59));

            if (b8_empty && c8_empty && d8_empty && c8_safe && d8_safe) {
                moves.push_back(Move{
                    60 % 8, 60 / 8,  // from e8
                    58 % 8, 58 / 8   // to   c8
                });
            }
        }
    }
}


uint64_t & Board::get_white_attacks() {
    return white_attacks;
}


uint64_t & Board::get_black_attacks() {
    return black_attacks;
}


bool Board::is_check(int king_row, int king_col, Color king_color) {
    // Color opponent_color = (king_color == Color::WHITE) ? Color::BLACK : Color::WHITE;
    uint64_t king_pos = 1ULL << (king_row * 8 + king_col);  // King's position as a bitboard

    // 1. Check for opposing pawns
    uint64_t opponent_pawns = (king_color == Color::WHITE) ? black_pawns : white_pawns;
    uint64_t pawn_attack_mask = (king_color == Color::WHITE) ? 
                                ((opponent_pawns >> 7) & ~0x0101010101010101ULL) | ((opponent_pawns >> 9) & ~0x8080808080808080ULL) :
                                ((opponent_pawns << 7) & ~0x8080808080808080ULL) | ((opponent_pawns << 9) & ~0x0101010101010101ULL);
    if (king_pos & pawn_attack_mask) {
        return true;  // King is under attack from an opposing pawn
    }

    uint64_t opponent_knights = (king_color == Color::WHITE) ? black_knights : white_knights;
    uint64_t knight_attack_mask = generate_knight_attack_mask(king_pos);  // Assume a pre-defined function for knight attack patterns
    if (knight_attack_mask & opponent_knights) {
        return true;  // King is under attack from an opposing knight
    }

    uint64_t opponent_rooks = (king_color == Color::WHITE) ? black_rooks : white_rooks;
    uint64_t opponent_queens = (king_color == Color::WHITE) ? black_queens : white_queens;
    uint64_t rook_attack_mask = generate_rook_attack_mask(king_pos, all_pieces);  // A pre-defined function for sliding rook attacks
    if ((rook_attack_mask & opponent_rooks) || (rook_attack_mask & opponent_queens)) {
        return true;  // King is under attack from a rook or queen
    }

    uint64_t opponent_bishops = (king_color == Color::WHITE) ? black_bishops : white_bishops;
    uint64_t bishop_attack_mask = generate_bishop_attack_mask(king_pos);  // A pre-defined function for sliding bishop attacks
    if ((bishop_attack_mask & opponent_bishops) || (bishop_attack_mask & opponent_queens)) {
        return true;  // King is under attack from a bishop or queen
    }

    uint64_t opponent_king = (king_color == Color::WHITE) ? black_kings : white_kings;
    uint64_t king_attack_mask = generate_king_attack_mask(king_pos);  // Assume a pre-defined function for king's attack mask
    if (king_attack_mask & opponent_king) {
        return true;  // King is adjacent to the opposing king
    }

    // No threats found
    return false;
}



uint64_t Board::generate_knight_attack_mask(uint64_t knight_pos) {

    int sq = __builtin_ctzll(knight_pos);
    return KNIGHT_ATTACKS[sq];
}


uint64_t Board::generate_bishop_attack_mask(uint64_t bishop_pos) {
    if (!bishop_pos) return 0ULL;
    int from_sq = __builtin_ctzll(bishop_pos);
    return magicAttacks.Bishop(all_pieces, from_sq);
}


uint64_t Board::generate_rook_attack_mask(uint64_t rook_pos, uint64_t all_pieces) {
    if (!rook_pos) return 0ULL;

    int from_square = __builtin_ctzll(rook_pos);
    // Magic bitboard library wants: occupancy, and the square index
    return magicAttacks.Rook(all_pieces, from_square);
}


uint64_t Board::generate_queen_attack_mask(uint64_t queen_pos, uint64_t occupancy) {
    if (!queen_pos) return 0ULL;
    int from_sq = __builtin_ctzll(queen_pos);
    return magicAttacks.Queen(occupancy, from_sq);
}



uint64_t Board::generate_king_attack_mask(uint64_t king_pos) {
    int sq = __builtin_ctzll(king_pos);
    return KING_ATTACKS[sq];
}

vector<Move> Board::generate_pseudo_legal_moves(Color color)
{
    vector<Move> moves;

    // Gather all pseudo-legal moves for 'color'.
    // This means: moves that do not land on friendly pieces and
    // follow the basic piece movement rules.
    // We do *not* worry about leaving your king in check or pinned pieces.

    if (color == Color::WHITE)
    {
        generate_pawn_moves(   white_pawns,   Color::WHITE, moves);
        generate_knight_moves( white_knights, Color::WHITE, moves);
        generate_rook_moves(   white_rooks,   Color::WHITE, moves);
        generate_bishop_moves( white_bishops, Color::WHITE, moves);
        generate_queen_moves(  white_queens,  Color::WHITE, moves);
        generate_king_moves(   white_kings,   Color::WHITE, moves);
    }
    else // color == Color::BLACK
    {
        generate_pawn_moves(   black_pawns,   Color::BLACK, moves);
        generate_knight_moves( black_knights, Color::BLACK, moves);
        generate_rook_moves(   black_rooks,   Color::BLACK, moves);
        generate_bishop_moves( black_bishops, Color::BLACK, moves);
        generate_queen_moves(  black_queens,  Color::BLACK, moves);
        generate_king_moves(   black_kings,   Color::BLACK, moves);
    }

    return moves;
}


bool Board::is_check(Color king_color) {
    uint64_t king_pos = (king_color == Color::WHITE) ? white_kings : black_kings;
    return is_check(king_pos, king_color);
}



bool Board::is_check(uint64_t king_pos, Color king_color) {
    uint64_t opponent_pawns = (king_color == Color::WHITE) ? black_pawns : white_pawns;
    uint64_t pawn_attack_mask = (king_color == Color::WHITE) ? 
                                ((opponent_pawns >> 7) & ~0x0101010101010101ULL) | ((opponent_pawns >> 9) & ~0x8080808080808080ULL) :
                                ((opponent_pawns << 7) & ~0x8080808080808080ULL) | ((opponent_pawns << 9) & ~0x0101010101010101ULL);
    if (king_pos & pawn_attack_mask) {
        return true;  
    }

    uint64_t opponent_knights = (king_color == Color::WHITE) ? black_knights : white_knights;
    uint64_t knight_attack_mask = generate_knight_attack_mask(king_pos); 
    if (knight_attack_mask & opponent_knights) {
        return true;  
    }

    uint64_t opponent_rooks = (king_color == Color::WHITE) ? black_rooks : white_rooks;
    uint64_t opponent_queens = (king_color == Color::WHITE) ? black_queens : white_queens;
    uint64_t rook_attack_mask = generate_rook_attack_mask(king_pos, all_pieces);
    if ((rook_attack_mask & opponent_rooks) || (rook_attack_mask & opponent_queens)) {
        return true;  // King is under attack from a rook or queen
    }

    uint64_t opponent_bishops = (king_color == Color::WHITE) ? black_bishops : white_bishops;
    uint64_t bishop_attack_mask = generate_bishop_attack_mask(king_pos); 
    if ((bishop_attack_mask & opponent_bishops) || (bishop_attack_mask & opponent_queens)) {
        return true; 
    }

    uint64_t opponent_king = (king_color == Color::WHITE) ? black_kings : white_kings;
    uint64_t king_attack_mask = generate_king_attack_mask(king_pos); 
    if (king_attack_mask & opponent_king) {
        return true; 
    }

    return false;
}


bool Board::is_stalemate(Color color) {
    bool is = is_checkmate(color);
    is = false;
    return is;
} //fixme


bool Board::is_checkmate(Color color) {

    if (!is_check(color)) 
        return false; 
    
    vector<Move> moves = generate_legal_moves(color);
    return moves.empty();
}


//  0b0001 = White can castle Kingside
//  0b0010 = White can castle Queenside
//  0b0100 = Black can castle Kingside
//  0b1000 = Black can castle Queenside
uint16_t Board::getCastlingRights() {
    uint16_t rights = 0;

    // White King-side castling
    if ((white_kings & (1ULL << 4)) && (white_rooks & (1ULL << 7))) {
        rights |= 0b0001;  // White King-side castling
    }

    // White Queen-side castling
    if ((white_kings & (1ULL << 4)) && (white_rooks & (1ULL << 0))) {
        rights |= 0b0010;  // White Queen-side castling
    }

    // Black King-side castling
    if ((black_kings & (1ULL << 60)) && (black_rooks & (1ULL << 63))) {
        rights |= 0b0100;  // Black King-side castling
    }

    // Black Queen-side castling
    if ((black_kings & (1ULL << 60)) && (black_rooks & (1ULL << 56))) {
        rights |= 0b1000;  // Black Queen-side castling
    }

    return rights;
}


void Board::set_turn_index(size_t index) {
    turn_index = index;
}

uint8_t Board::getEnPassantSquare() const {
    // Check if an en passant square exists
    if (en_passant_square >= 0 && en_passant_square < 64) {
        return static_cast<uint8_t>(en_passant_square);
    }

    // If no en passant square is valid, return an invalid value
    return 0xFF;
}


static const int KING_RAYS[8] = {8, -8, 1, -1, 9, 7, -7, -9};

void Board::compute_attack_masks() {
    // Compute white_attacks and black_attacks from the current board position.

    white_attacks = 0ULL;
    black_attacks = 0ULL;

    // White Pawns attack
    // White pawns attack one square diagonally up-left or up-right
    uint64_t w_p = white_pawns;
    white_attacks |= ((w_p << 7) & ~0x8080808080808080ULL);
    white_attacks |= ((w_p << 9) & ~0x0101010101010101ULL);

    // Black Pawns attack
    // Black pawns attack one square diagonally down-left or down-right
    uint64_t b_p = black_pawns;
    black_attacks |= ((b_p >> 7) & ~0x0101010101010101ULL);
    black_attacks |= ((b_p >> 9) & ~0x8080808080808080ULL);

    // Knights
    uint64_t w_n = white_knights;
    while (w_n) {
        int sq = __builtin_ctzll(w_n);
        white_attacks |= KNIGHT_ATTACKS[sq];
        w_n &= w_n - 1;
    }
    uint64_t b_n = black_knights;
    while (b_n) {
        int sq = __builtin_ctzll(b_n);
        black_attacks |= KNIGHT_ATTACKS[sq];
        b_n &= b_n - 1;
    }

    // Kings
    if (white_kings) {
        int sq = __builtin_ctzll(white_kings);
        white_attacks |= KING_ATTACKS[sq];
    }
    if (black_kings) {
        int sq = __builtin_ctzll(black_kings);
        black_attacks |= KING_ATTACKS[sq];
    }

    // Rooks + Queens (white)
    uint64_t w_rq = white_rooks | white_queens;
    while (w_rq) {
        int sq = __builtin_ctzll(w_rq);
        white_attacks |= generate_rook_attack_mask(1ULL << sq, all_pieces);
        w_rq &= w_rq - 1;
    }

    // Rooks + Queens (black)
    uint64_t b_rq = black_rooks | black_queens;
    while (b_rq) {
        int sq = __builtin_ctzll(b_rq);
        black_attacks |= generate_rook_attack_mask(1ULL << sq, all_pieces);
        b_rq &= b_rq - 1;
    }

    // Bishops + Queens (white)
    uint64_t w_bq = white_bishops | white_queens;
    while (w_bq) {
        int sq = __builtin_ctzll(w_bq);
        white_attacks |= generate_bishop_attack_mask(1ULL << sq);
        w_bq &= w_bq - 1;
    }

    // Bishops + Queens (black)
    uint64_t b_bq = black_bishops | black_queens;
    while (b_bq) {
        int sq = __builtin_ctzll(b_bq);
        black_attacks |= generate_bishop_attack_mask(1ULL << sq);
        b_bq &= b_bq - 1;
    }
}


void Board::detect_pins(Color us, int king_sq) {
    // Reset pinned info
    pinned_pieces = 0ULL;
    for (int i = 0; i < 64; i++) 
        pin_line[i] = 0ULL;

    uint64_t own_pieces = (us == Color::WHITE) ? all_white_pieces : all_black_pieces;
    uint64_t opp_rooks   = (us == Color::WHITE) ? black_rooks   : white_rooks;
    uint64_t opp_bishops = (us == Color::WHITE) ? black_bishops : white_bishops;
    uint64_t opp_queens  = (us == Color::WHITE) ? black_queens  : white_queens;

    for (int dir = 0; dir < 8; dir++) {
        int d = KING_RAYS[dir];
        int sq = king_sq;
        bool found_own = false;
        int own_piece_sq = -1;

        while (true) {
            sq += d;
            if (sq < 0 || sq >= 64) break;
            // Check file wrapping
            int prev = sq - d;
            if ((d == 1 || d == -1 || d == 9 || d == 7 || d == -7 || d == -9)) {
                int file_diff = abs((sq % 8) - (prev % 8));
                if (file_diff != 1) break;
            }

            uint64_t bit = 1ULL << sq;
            if (all_pieces & bit) {
                // Hit a piece
                if (!found_own) {
                    // First piece along this ray
                    if (own_pieces & bit) {
                        found_own = true;
                        own_piece_sq = sq;
                    } else {
                        // Opponent piece first means no pin here
                        break;
                    }
                } else {
                    // Second piece encountered
                    bool rook_dir = (d == 8 || d == -8 || d == 1 || d == -1);
                    bool bishop_dir = (d == 9 || d == 7 || d == -7 || d == -9);

                    uint64_t bit2 = 1ULL << sq;
                    bool is_sliding_attacker = false;

                    // If us == WHITE, we look for black sliding pieces in correct directions
                    // If us == BLACK, we look for white sliding pieces
                    if (rook_dir && ((opp_rooks | opp_queens) & bit2)) {
                        is_sliding_attacker = true;
                    } else if (bishop_dir && ((opp_bishops | opp_queens) & bit2)) {
                        is_sliding_attacker = true;
                    }

                    if (is_sliding_attacker && own_piece_sq != -1) {
                        pinned_pieces |= (1ULL << own_piece_sq);

                        // Build pin line (king -> attacker)
                        uint64_t line_mask = 0ULL;
                        int sq_trace = king_sq;
                        while (sq_trace != sq) {
                            sq_trace += d;
                            if (sq_trace != sq && sq_trace != own_piece_sq) {
                                line_mask |= (1ULL << sq_trace);
                            }
                        }
                        line_mask |= (1ULL << own_piece_sq); 
                        line_mask |= (1ULL << sq);
                        pin_line[own_piece_sq] = line_mask;
                    }

                    break; // Stop after second piece
                }
            }
        }
    }
}



bool Board::is_pinned_piece(int sq) const {
    return (pinned_pieces & (1ULL << sq)) != 0ULL;
}

bool Board::is_within_bounds(int rank, int file) {
    return (rank >= 0 && rank < 8 &&
            file >= 0 && file < 8);
}


std::vector<Move> Board::generate_legal_moves(Color color) {
    // Compute attack masks for both sides first

// 1) Recompute all attack masks:
    //    - who attacks what squares
    compute_attack_masks();

    // 2) Identify our king’s square so we can detect pins
    uint64_t king_bb = (color == Color::WHITE) ? white_kings : black_kings;
    if (king_bb == 0ULL) {
        // No king? (should never happen, but just in case)
        return {};
    }
    int king_sq = __builtin_ctzll(king_bb);

    // 3) Detect pinned pieces (stores pins in pinned_pieces, pin_line, etc.)
    detect_pins(color, king_sq);

    // 4) Generate *all* pseudo-legal moves
    std::vector<Move> pseudo_moves = generate_pseudo_legal_moves(color);

    // 5) Filter them for true legality
    std::vector<Move> legal_moves;
    filter_moves_for_legality(pseudo_moves, color, legal_moves);

    return legal_moves;
}

void Board::filter_moves_for_legality(
    const std::vector<Move> & pseudo_moves,
    Color color,
    std::vector<Move> & legal_moves
)
{
    // Identify the squares the *opponent* attacks (so we know if the king steps into check)
    uint64_t enemy_attacks = (color == Color::WHITE) ? black_attacks : white_attacks;

    for (const auto & move : pseudo_moves)
    {
        int from_sq = move.start_rank * 8 + move.start_file;
        int to_sq   = move.end_rank   * 8 + move.end_file;

        // 1) If pinned, must move along the pin line
        if (is_pinned_piece(from_sq)) {
            uint64_t to_bit = (1ULL << to_sq);
            if ((pin_line[from_sq] & to_bit) == 0ULL) {
                // The “to” square is not in the pin line => illegal
                continue;
            }
        }

        // 2) If the piece is the king, don’t step onto an attacked square
        Piece from_piece = getPiece(move.start_rank, move.start_file);
        if (from_piece.getType() == PieceType::KING) {
            uint64_t to_bit = (1ULL << to_sq);
            if (to_bit & enemy_attacks) {
                // King would move into check
                continue;
            }
        }

        // 3) Temporarily make the move
        Piece captured_piece = getPiece(move.end_rank, move.end_file);
        make_move(move);

        // 4) If after that, we’re still in check, discard the move
        if (is_check(color)) {
            undo_move(move, captured_piece);
            continue;
        }

        // 5) Otherwise, keep it
        undo_move(move, captured_piece);
        legal_moves.push_back(move);
    }
}


void Board::print_position() {

    string toPrint = "";
    // 1. Piece Placement
    for (int i = 7; i >= 0; --i) {
        int emptyCount = 0; // Track consecutive empty squares
        for (int j = 0; j < 8; ++j) {
            string pieceChar = board[static_cast<size_t>(i)][static_cast<size_t>(j)].return_string();
            if (pieceChar == "") { // Assuming '.' represents an empty square
                ++emptyCount;
            } else {
                if (emptyCount > 0) {
                    toPrint += to_string(emptyCount); // Add count of empty squares
                    emptyCount = 0;
                }
                toPrint += pieceChar;
            }
        }
        if (emptyCount > 0) {
            toPrint += to_string(emptyCount); // Add remaining empty squares at the end of the row
        }
        if (i != 0) { // Add a '/' after each row except the last one
            toPrint += "/";
        }
    }

    toPrint += " ";
    if (get_color() == Color::WHITE) 
        toPrint += "w";
    else 
        toPrint += "b";
    
    // Output the FEN string
    cout << toPrint << endl;
}


Color Board::get_color() {
    if ((turn_index % 2) == 0) 
        return Color::WHITE;
    else 
        return Color::BLACK;
}


BoardState Board::get_current_board_state() {
    BoardState toreturn = track_position();
    return toreturn;
}


size_t Board::get_turn_index() {
    return turn_index;
}


BoardState Board::track_position() {
    BoardState state = {
        white_pawns,    
        white_knights,
        white_bishops,
        white_rooks,
        white_queens,
        white_kings,
        black_pawns,
        black_knights,
        black_bishops,
        black_rooks,
        black_queens,
        black_kings,
        getCastlingRights(),
        getEnPassantSquare(),
    };

    return state;
}


bool Board::is_square_attacked(int square, Color attacker_color) const {
    uint64_t square_bit = 1ULL << square;

    // Pawns
    uint64_t attacker_pawns = (attacker_color == Color::WHITE) ? white_pawns : black_pawns;
    uint64_t pawn_attacks = (attacker_color == Color::WHITE) ?
                            ((attacker_pawns << 7) & ~FILE_H) |
                            ((attacker_pawns << 9) & ~FILE_A) :
                            ((attacker_pawns >> 7) & ~FILE_H) |
                            ((attacker_pawns >> 9) & ~FILE_A);
    if (pawn_attacks & square_bit)
        return true;

    // Knights
    uint64_t attacker_knights = (attacker_color == Color::WHITE) ? white_knights : black_knights;
    uint64_t knight_attacks = KNIGHT_ATTACKS[square];
    if (knight_attacks & attacker_knights)
        return true;

    // Bishops and Queens (Diagonal)
    uint64_t attacker_bishops = (attacker_color == Color::WHITE) ? white_bishops : black_bishops;
    uint64_t attacker_queens_bishop = (attacker_color == Color::WHITE) ? white_queens : black_queens;
    uint64_t bishop_attacks = magicAttacks.Bishop(all_pieces, square);
    if ((bishop_attacks & attacker_bishops) || (bishop_attacks & attacker_queens_bishop))
        return true;

    // Rooks and Queens (Straight)
    uint64_t attacker_rooks = (attacker_color == Color::WHITE) ? white_rooks : black_rooks;
    uint64_t attacker_queens_rook = (attacker_color == Color::WHITE) ? white_queens : black_queens;
    uint64_t rook_attacks = magicAttacks.Rook(all_pieces, square);
    if ((rook_attacks & attacker_rooks) || (rook_attacks & attacker_queens_rook))
        return true;

    // Kings
    uint64_t attacker_kings = (attacker_color == Color::WHITE) ? white_kings : black_kings;
    uint64_t king_attacks = KING_ATTACKS[square];
    if (king_attacks & attacker_kings)
        return true;

    return false;
}

// **New Method: Find All Checking Pieces Against the King**
vector<int> Board::find_checking_pieces(int king_sq, Color attacker_color) const {
    vector<int> checkers;
    uint64_t king_bit = 1ULL << king_sq;

    // Pawns
    uint64_t attacker_pawns = (attacker_color == Color::WHITE) ? white_pawns : black_pawns;
    uint64_t pawn_attacks = (attacker_color == Color::WHITE) ?
                            ((attacker_pawns << 7) & ~FILE_H) |
                            ((attacker_pawns << 9) & ~FILE_A) :
                            ((attacker_pawns >> 7) & ~FILE_H) |
                            ((attacker_pawns >> 9) & ~FILE_A);
    uint64_t pawn_checks = pawn_attacks & king_bit;
    while (pawn_checks) {
        int checker_sq = __builtin_ctzll(pawn_checks);
        checkers.push_back(checker_sq);
        pawn_checks &= pawn_checks - 1;
    }

    // Knights
    uint64_t attacker_knights = (attacker_color == Color::WHITE) ? white_knights : black_knights;
    uint64_t knight_attacks = KNIGHT_ATTACKS[king_sq] & attacker_knights;
    while (knight_attacks) {
        int checker_sq = __builtin_ctzll(knight_attacks);
        checkers.push_back(checker_sq);
        knight_attacks &= knight_attacks - 1;
    }

    // Bishops and Queens (Diagonal)
    uint64_t attacker_bishops = (attacker_color == Color::WHITE) ? white_bishops : black_bishops;
    uint64_t attacker_queens_bishop = (attacker_color == Color::WHITE) ? white_queens : black_queens;
    uint64_t bishop_attacks = magicAttacks.Bishop(all_pieces, king_sq);
    uint64_t bishop_checks = (bishop_attacks & attacker_bishops) | (bishop_attacks & attacker_queens_bishop);
    while (bishop_checks) {
        int checker_sq = __builtin_ctzll(bishop_checks);
        checkers.push_back(checker_sq);
        bishop_checks &= bishop_checks - 1;
    }

    // Rooks and Queens (Straight)
    uint64_t attacker_rooks = (attacker_color == Color::WHITE) ? white_rooks : black_rooks;
    uint64_t attacker_queens_rook = (attacker_color == Color::WHITE) ? white_queens : black_queens;
    uint64_t rook_attacks = magicAttacks.Rook(all_pieces, king_sq);
    uint64_t rook_checks = (rook_attacks & attacker_rooks) | (rook_attacks & attacker_queens_rook);
    while (rook_checks) {
        int checker_sq = __builtin_ctzll(rook_checks);
        checkers.push_back(checker_sq);
        rook_checks &= rook_checks - 1;
    }

    return checkers;
}

// **New Method: Determine Direction Between Two Squares (Used for Blocking)**
int Board::get_direction(int from_sq, int to_sq) const {
    int from_rank = from_sq / 8;
    int from_file = from_sq % 8;
    int to_rank = to_sq / 8;
    int to_file = to_sq % 8;

    int dr = to_rank - from_rank;
    int dc = to_file - from_file;

    if (dr == 0) {
        return (dc > 0) ? 1 : -1;
    }
    if (dc == 0) {
        return (dr > 0) ? 8 : -8;
    }
    if (abs(dr) == abs(dc)) {
        if (dr > 0 && dc > 0) return 9;
        if (dr > 0 && dc < 0) return 7;
        if (dr < 0 && dc > 0) return -7;
        if (dr < 0 && dc < 0) return -9;
    }
    return 0; // Not aligned
}

// **New Method: Generate Moves to Capture the Checking Piece**
void Board::generate_captures(Color color, int checker_sq, vector<Move> & moves) {
    // Identify all pieces of 'color' that can capture the piece at 'checker_sq'
    uint64_t target_bit = 1ULL << checker_sq;

    // Iterate through all pieces of 'color' and see if they can attack 'checker_sq'
    // This includes all piece types except the king (handled separately)

    // Pawns
    uint64_t pawns = (color == Color::WHITE) ? white_pawns : black_pawns;
    uint64_t pawn_captures = (color == Color::WHITE) ?
                            ((pawns << 7) & ~FILE_H) |
                            ((pawns << 9) & ~FILE_A) :
                            ((pawns >> 7) & ~FILE_H) |
                            ((pawns >> 9) & ~FILE_A);
    uint64_t capturing_pawns = pawn_captures & target_bit;
    while (capturing_pawns) {
        int to_sq = __builtin_ctzll(capturing_pawns);
        capturing_pawns &= capturing_pawns - 1;
        int from_sq = (color == Color::WHITE) ? to_sq - 7 : to_sq + 7;
        moves.emplace_back(Move{from_sq % 8, from_sq / 8, to_sq % 8, to_sq / 8});
    }

    // Knights
    uint64_t knights = (color == Color::WHITE) ? white_knights : black_knights;
    uint64_t knight_attacks = KNIGHT_ATTACKS[checker_sq] & knights;
    while (knight_attacks) {
        int from_sq = __builtin_ctzll(knight_attacks);
        knight_attacks &= knight_attacks - 1;
        moves.emplace_back(Move{from_sq % 8, from_sq / 8, checker_sq % 8, checker_sq / 8});
    }

    // Bishops and Queens (Diagonal)
    uint64_t bishops = (color == Color::WHITE) ? white_bishops : black_bishops;
    uint64_t queens_bishop = (color == Color::WHITE) ? white_queens : black_queens;
    uint64_t bishop_attacks = magicAttacks.Bishop(all_pieces, checker_sq);
    uint64_t capturing_bishops = (bishop_attacks & bishops) | (bishop_attacks & queens_bishop);
    while (capturing_bishops) {
        int from_sq = __builtin_ctzll(capturing_bishops);
        capturing_bishops &= capturing_bishops - 1;
        moves.emplace_back(Move{from_sq % 8, from_sq / 8, checker_sq % 8, checker_sq / 8});
    }

    // Rooks and Queens (Straight)
    uint64_t rooks = (color == Color::WHITE) ? white_rooks : black_rooks;
    uint64_t queens_rook = (color == Color::WHITE) ? white_queens : black_queens;
    uint64_t rook_attacks = magicAttacks.Rook(all_pieces, checker_sq);
    uint64_t capturing_rooks = (rook_attacks & rooks) | (rook_attacks & queens_rook);
    while (capturing_rooks) {
        int from_sq = __builtin_ctzll(capturing_rooks);
        capturing_rooks &= capturing_rooks - 1;
        moves.emplace_back(Move{from_sq % 8, from_sq / 8, checker_sq % 8, checker_sq / 8});
    }
}

uint64_t Board::get_between_squares(int from_sq, int to_sq) const {
    uint64_t between = 0ULL;
    int direction = get_direction(from_sq, to_sq);
    if (direction == 0)
        return between; // Not a straight line (no squares in between)
    
    int current_sq = from_sq + direction;
    while (current_sq != to_sq) {
        between |= (1ULL << current_sq);
        current_sq += direction;
    }
    return between;
}



// **New Method: Generate Blocking Moves Between King and Checking Piece**
void Board::generate_blocks(Color color, int checker_sq, vector<Move> & moves) {
    // Only applicable for sliding pieces
    // Find squares between king and checker

    int king_sq = (color == Color::WHITE) ? __builtin_ctzll(white_kings) : __builtin_ctzll(black_kings);
    uint64_t between = get_between_squares(king_sq, checker_sq);
    if (between == 0ULL)
        return; // Not a sliding piece or adjacent

    // Iterate over all squares in 'between' and generate moves to those squares
    for (int sq = 0; sq < 64; ++sq) {
        if (between & (1ULL << sq)) {
            // Find all pieces of 'color' that can move to 'sq'
            // Exclude king

            // Pawns
            uint64_t pawns = (color == Color::WHITE) ? white_pawns : black_pawns;
            // Pawns can only move forward
            if (color == Color::WHITE) {
                // Single push
                if (sq >= 8 && !(all_pieces & (1ULL << sq))) {
                    uint64_t pawn_from = (1ULL << (sq - 8)) & pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
                // Double push from starting rank
                if (sq >= 16 && (sq % 8) == (sq - 8) % 8 &&
                    !(all_pieces & (1ULL << sq)) && !(all_pieces & (1ULL << (sq - 8)))) {
                    uint64_t pawn_from = (1ULL << (sq - 16)) & pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
                // Captures
                if (sq % 8 > 0 && (sq - 7) >= 0) {
                    uint64_t pawn_from = (1ULL << (sq - 7)) & pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
                if (sq % 8 < 7 && (sq - 9) >= 0) {
                    uint64_t pawn_from = (1ULL << (sq - 9)) & pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
            }
            else { // Color::BLACK
                // Single push
                if (sq <= 55 && !(all_pieces & (1ULL << sq))) {
                    uint64_t pawn_from = (1ULL << (sq + 8)) & black_pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
                // Double push from starting rank
                if (sq <= 47 && (sq % 8) == (sq + 8) % 8 &&
                    !(all_pieces & (1ULL << sq)) && !(all_pieces & (1ULL << (sq + 8)))) {
                    uint64_t pawn_from = (1ULL << (sq + 16)) & black_pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
                // Captures
                if (sq % 8 > 0 && (sq + 7) < 64) {
                    uint64_t pawn_from = (1ULL << (sq + 7)) & black_pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
                if (sq % 8 < 7 && (sq + 9) < 64) {
                    uint64_t pawn_from = (1ULL << (sq + 9)) & black_pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
            }
        }
    }
}


// **New Method: Generate Legal King Moves Only (Considering Attacks)**
void Board::generate_king_moves(uint64_t kings, Color color, vector<Move> & moves, bool double_check) {
    if (kings == 0ULL) return;

    int king_sq = __builtin_ctzll(kings); // Assuming only one king
    uint64_t attack_mask = KING_ATTACKS[king_sq];
    
    uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;
    uint64_t valid_moves = attack_mask & ~friendlies;

    // Exclude squares under attack
    Color opponent_color = (color == Color::WHITE) ? Color::BLACK : Color::WHITE;
    uint64_t enemy_attacks = (opponent_color == Color::WHITE) ? white_attacks : black_attacks;
    valid_moves &= ~enemy_attacks;

    // Each set bit in valid_moves is a potential destination
    while (valid_moves) {
        int to_sq = __builtin_ctzll(valid_moves);
        valid_moves &= valid_moves - 1;

        moves.emplace_back(Move{
            king_sq % 8, king_sq / 8,
            to_sq % 8,   to_sq / 8
        });
    }

    if (double_check) {
        // In double check, no castling is allowed
        return;
    }

    // Handle castling
    uint16_t rights = getCastlingRights();
    if (color == Color::WHITE) {
        // Kingside Castling
        if (rights & 0b0001) { // White can castle kingside
            // Squares f1 and g1 must be empty
            if (!(all_pieces & ((1ULL << 5) | (1ULL << 6)))) {
                // Squares f1 and g1 must not be under attack
                if (!is_square_attacked(5, opponent_color) && !is_square_attacked(6, opponent_color)) {
                    moves.emplace_back(Move{4, 0, 6, 0}); // e1 to g1
                }
            }
        }

        // Queenside Castling
        if (rights & 0b0010) { // White can castle queenside
            // Squares b1, c1, and d1 must be empty
            if (!(all_pieces & ((1ULL << 1) | (1ULL << 2) | (1ULL << 3)))) {
                // Squares c1 and d1 must not be under attack
                if (!is_square_attacked(2, opponent_color) && !is_square_attacked(3, opponent_color)) {
                    moves.emplace_back(Move{4, 0, 2, 0}); // e1 to c1
                }
            }
        }
    }
    else { // Color::BLACK
        // Kingside Castling
        if (rights & 0b0100) { // Black can castle kingside
            // Squares f8 and g8 must be empty
            if (!(all_pieces & ((1ULL << 61) | (1ULL << 62)))) {
                // Squares f8 and g8 must not be under attack
                if (!is_square_attacked(61, opponent_color) && !is_square_attacked(62, opponent_color)) {
                    moves.emplace_back(Move{60, 7, 62, 7}); // e8 to g8
                }
            }
        }

        // Queenside Castling
        if (rights & 0b1000) { // Black can castle queenside
            // Squares b8, c8, and d8 must be empty
            if (!(all_pieces & ((1ULL << 57) | (1ULL << 58) | (1ULL << 59)))) {
                // Squares c8 and d8 must not be under attack
                if (!is_square_attacked(58, opponent_color) && !is_square_attacked(59, opponent_color)) {
                    moves.emplace_back(Move{60, 7, 58, 7}); // e8 to c8
                }
            }
        }
    }
}

bool Board::is_sliding_piece(PieceType type) const {
    return (type == PieceType::BISHOP || type == PieceType::ROOK || type == PieceType::QUEEN);
}

