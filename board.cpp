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

    // Clear history stacks so set_from_fen() doesn't inherit stale EP/castling state.
    en_passant_stack = stack<int>();
    promotion_stack = stack<int>();
    castling_stack = stack<bool>();
    castling_rights_stack = stack<uint16_t>();

    castling_rights = 0;
    en_passant_square = 64;
}
    

void Board::generate_board() {

    board.resize(8, vector<Piece>(8, Piece(PieceType::NONE, Color::NONE)));

    // H8 is MSB and A1 is LSB
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

    // Must set these here: an older version left rights at 0 and EP uninitialized.
    castling_rights = WHITE_KINGSIDE | WHITE_QUEENSIDE |
                      BLACK_KINGSIDE | BLACK_QUEENSIDE;
    en_passant_square = 64;
    turn_index = 0;
    white_attacks = black_attacks = pinned_pieces = 0ULL;
}


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

bool Board::is_within_bounds(int row, int col) const {
    return (row >= 0 && row < 8 && col >= 0 && col < 8);
}


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

void Board::make_move(const Move & move) {
    if (move.start_rank < 0 || move.start_rank >= 8 || 
        move.start_file < 0 || move.start_file >= 8 ||
        move.end_rank < 0 || move.end_rank >= 8 ||
        move.end_file < 0 || move.end_file >= 8) 
    {
        cout << "invalid move\n";
        assert(false);
    }

    Piece piece = board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)];
    Piece captured_piece = board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)];

    // Push rights every make so undo can restore them 1:1.
    castling_rights_stack.push(castling_rights);

    bool castled = check_castling(piece, move);

    if (castled) {
        // King moved: spend both castling rights for this color.
        castling_rights &= static_cast<uint16_t>(
            piece.getColor() == Color::WHITE
                ? ~(WHITE_KINGSIDE | WHITE_QUEENSIDE)
                : ~(BLACK_KINGSIDE | BLACK_QUEENSIDE));
        // Clear EP and push it — every make must push exactly one EP entry.
        // Older code left the opponent's EP target alive after castling.
        en_passant_square = 64;
        en_passant_stack.push(en_passant_square);
        return;
    }

    castling_stack.push(false);

    // Revoke castling rights on king/rook moves and rook captures.
    // Used to keep rights forever, so castling after a captured rook could wipe the corner square.
    {
        auto revoke_corner = [this](int square) {
            switch (square) {
                case 0:  castling_rights &= static_cast<uint16_t>(~WHITE_QUEENSIDE); break; // a1
                case 7:  castling_rights &= static_cast<uint16_t>(~WHITE_KINGSIDE);  break; // h1
                case 56: castling_rights &= static_cast<uint16_t>(~BLACK_QUEENSIDE); break; // a8
                case 63: castling_rights &= static_cast<uint16_t>(~BLACK_KINGSIDE);  break; // h8
                default: break;
            }
        };
        if (piece.getType() == PieceType::KING) {
            castling_rights &= static_cast<uint16_t>(
                piece.getColor() == Color::WHITE
                    ? ~(WHITE_KINGSIDE | WHITE_QUEENSIDE)
                    : ~(BLACK_KINGSIDE | BLACK_QUEENSIDE));
        }
        if (piece.getType() == PieceType::ROOK) {
            revoke_corner(move.start_rank * 8 + move.start_file);
        }
        if (captured_piece.getType() == PieceType::ROOK) {
            revoke_corner(move.end_rank * 8 + move.end_file);
        }
    }

    board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)] = Piece(PieceType::NONE, Color::NONE);
    board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)] = piece;

    uint64_t start_bit = 1ULL << (move.start_rank * 8 + move.start_file);
    uint64_t end_bit = 1ULL << (move.end_rank * 8 + move.end_file);
    

    Color c = piece.getColor();

    if (!en_passant_stack.empty()) {
        int en = en_passant_stack.top();
        if ((en != 64) && piece.getType() == PieceType::PAWN && 
            ((move.end_rank * 8 + move.end_file) == en)) {

            int capture_rank = (c == Color::WHITE) ? move.end_rank - 1 : move.end_rank + 1;
            int capture_file = move.end_file;

            board[static_cast<size_t>(capture_rank)][static_cast<size_t>(capture_file)] = Piece(PieceType::NONE, Color::NONE);

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


    if (piece.getType() == PieceType::PAWN &&
        ((c == Color::WHITE && move.end_rank == 7) ||
         (c == Color::BLACK && move.end_rank == 0))) {
        // promotion == NONE still means queen (old Move construction).
        PieceType promoted = move.promotion;
        if (promoted != PieceType::QUEEN && promoted != PieceType::ROOK &&
            promoted != PieceType::BISHOP && promoted != PieceType::KNIGHT) {
            promoted = PieceType::QUEEN;
        }
        const int endSquareIndex = move.end_rank * 8 + move.end_file;

        board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)] =
            Piece(promoted, c);

        // Swap pawn bitboard for the promoted piece.
        if (c == Color::WHITE) white_pawns &= ~end_bit; else black_pawns &= ~end_bit;
        switch (promoted) {
            case PieceType::KNIGHT:
                (c == Color::WHITE ? white_knights : black_knights) |= end_bit;
                break;
            case PieceType::BISHOP:
                (c == Color::WHITE ? white_bishops : black_bishops) |= end_bit;
                break;
            case PieceType::ROOK:
                (c == Color::WHITE ? white_rooks : black_rooks) |= end_bit;
                break;
            default: // QUEEN
                (c == Color::WHITE ? white_queens : black_queens) |= end_bit;
                break;
        }

        promotion_stack.push(endSquareIndex);
    }
    else {
        promotion_stack.push(64);
    }


    turn_index++;
    all_white_pieces = white_pawns | white_knights | white_rooks | white_bishops | white_queens | white_kings;
    all_black_pieces = black_pawns | black_knights | black_rooks | black_bishops | black_queens | black_kings;
    all_pieces = all_white_pieces | all_black_pieces;
}


bool Board::check_castling(Piece & p, const Move & m) {
    // White king: e1->g1 or e1->c1
    if (p.getType() == PieceType::KING && p.getColor() == Color::WHITE && 
        m.start_rank == 0 && m.start_file == 4) {
        // White kingside — only if the rook is still on h1
        if (m.end_rank == 0 && m.end_file == 6 &&
            board[0][7].getType() == PieceType::ROOK &&
            board[0][7].getColor() == Color::WHITE) {
            board[0][4] = Piece(PieceType::NONE, Color::NONE);
            board[0][6] = Piece(PieceType::KING, Color::WHITE);

            board[0][7] = Piece(PieceType::NONE, Color::NONE);
            board[0][5] = Piece(PieceType::ROOK, Color::WHITE);

            uint64_t king_start_bit = 1ULL << (0 * 8 + 4);
            white_kings &= ~king_start_bit;
            uint64_t rook_start_bit = 1ULL << (0 * 8 + 7);
            white_rooks &= ~rook_start_bit;

            uint64_t king_end_bit = 1ULL << (0 * 8 + 6);
            white_kings |= king_end_bit;
            uint64_t rook_end_bit = 1ULL << (0 * 8 + 5);
            white_rooks |= rook_end_bit;

            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            turn_index++;
            castling_stack.push(true);  // push "true" for castling
            return true;
        }

        // White queenside — only if the rook is still on a1
        if (m.end_rank == 0 && m.end_file == 2 &&
            board[0][0].getType() == PieceType::ROOK &&
            board[0][0].getColor() == Color::WHITE) {
            board[0][4] = Piece(PieceType::NONE, Color::NONE);
            board[0][2] = Piece(PieceType::KING, Color::WHITE);

            board[0][0] = Piece(PieceType::NONE, Color::NONE);
            board[0][3] = Piece(PieceType::ROOK, Color::WHITE);

            uint64_t king_start_bit = 1ULL << (0 * 8 + 4);
            white_kings &= ~king_start_bit;
            uint64_t rook_start_bit = 1ULL << (0 * 8 + 0);
            white_rooks &= ~rook_start_bit;

            uint64_t king_end_bit = 1ULL << (0 * 8 + 2);
            white_kings |= king_end_bit;
            uint64_t rook_end_bit = 1ULL << (0 * 8 + 3);
            white_rooks |= rook_end_bit;

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
        // Black kingside — only if the rook is still on h8
        if (m.end_rank == 7 && m.end_file == 6 &&
            board[7][7].getType() == PieceType::ROOK &&
            board[7][7].getColor() == Color::BLACK)
        {
            board[7][4] = Piece(PieceType::NONE, Color::NONE);
            board[7][6] = Piece(PieceType::KING, Color::BLACK);

            board[7][7] = Piece(PieceType::NONE, Color::NONE);
            board[7][5] = Piece(PieceType::ROOK, Color::BLACK);

            uint64_t king_start_bit = 1ULL << (7 * 8 + 4);
            black_kings &= ~king_start_bit;
            uint64_t rook_start_bit = 1ULL << (7 * 8 + 7);
            black_rooks &= ~rook_start_bit;

            uint64_t king_end_bit = 1ULL << (7 * 8 + 6);
            black_kings |= king_end_bit;
            uint64_t rook_end_bit = 1ULL << (7 * 8 + 5);
            black_rooks |= rook_end_bit;

            all_white_pieces = (white_pawns | white_knights | white_rooks |
                                white_bishops | white_queens | white_kings);
            all_black_pieces = (black_pawns | black_knights | black_rooks |
                                black_bishops | black_queens | black_kings);
            all_pieces = all_white_pieces | all_black_pieces;

            turn_index++;
            castling_stack.push(true);
            return true;
        }

        // Black queenside — only if the rook is still on a8
        if (m.end_rank == 7 && m.end_file == 2 &&
            board[7][0].getType() == PieceType::ROOK &&
            board[7][0].getColor() == Color::BLACK)
        {
            board[7][4] = Piece(PieceType::NONE, Color::NONE);
            board[7][2] = Piece(PieceType::KING, Color::BLACK);

            board[7][0] = Piece(PieceType::NONE, Color::NONE);
            board[7][3] = Piece(PieceType::ROOK, Color::BLACK);

            uint64_t king_start_bit = 1ULL << (7 * 8 + 4);
            black_kings &= ~king_start_bit;
            uint64_t rook_start_bit = 1ULL << (7 * 8 + 0);
            black_rooks &= ~rook_start_bit;

            uint64_t king_end_bit = 1ULL << (7 * 8 + 2);
            black_kings |= king_end_bit;
            uint64_t rook_end_bit = 1ULL << (7 * 8 + 3);
            black_rooks |= rook_end_bit;

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


void Board::undo_move(const Move & move, const Piece & captured_piece) {

    bool was_castling = castling_stack.top();
    castling_stack.pop();

    // Rights were pushed on every make_move.
    castling_rights = castling_rights_stack.top();
    castling_rights_stack.pop();

    // Undo castling king+rook placement:
    if (was_castling) {
        // Pop the EP entry castling pushed.
        en_passant_stack.pop();
        en_passant_square =
            en_passant_stack.empty() ? 64 : en_passant_stack.top();

        // White kingside? e1->g1
        if (move.start_rank == 0 && move.start_file == 4 &&
            move.end_rank == 0 && move.end_file == 6) 
        {
            board[0][4] = Piece(PieceType::KING, Color::WHITE);
            board[0][6] = Piece(PieceType::NONE, Color::NONE);

            board[0][7] = Piece(PieceType::ROOK, Color::WHITE);
            board[0][5] = Piece(PieceType::NONE, Color::NONE);

            uint64_t king_end_bit = 1ULL << (0 * 8 + 6);
            white_kings &= ~king_end_bit;
            uint64_t rook_end_bit = 1ULL << (0 * 8 + 5);
            white_rooks &= ~rook_end_bit;

            uint64_t king_start_bit = 1ULL << (0 * 8 + 4);
            white_kings |= king_start_bit;
            uint64_t rook_start_bit = 1ULL << (0 * 8 + 7);
            white_rooks |= rook_start_bit;

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
            board[0][4] = Piece(PieceType::KING, Color::WHITE);
            board[0][2] = Piece(PieceType::NONE, Color::NONE);

            board[0][0] = Piece(PieceType::ROOK, Color::WHITE);
            board[0][3] = Piece(PieceType::NONE, Color::NONE);

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
            board[7][4] = Piece(PieceType::KING, Color::BLACK);
            board[7][6] = Piece(PieceType::NONE, Color::NONE);

            board[7][7] = Piece(PieceType::ROOK, Color::BLACK);
            board[7][5] = Piece(PieceType::NONE, Color::NONE);

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
            board[7][4] = Piece(PieceType::KING, Color::BLACK);
            board[7][2] = Piece(PieceType::NONE, Color::NONE);

            board[7][0] = Piece(PieceType::ROOK, Color::BLACK);
            board[7][3] = Piece(PieceType::NONE, Color::NONE);

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

    board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)] = piece;
    board[static_cast<size_t>(move.end_rank)][static_cast<size_t>(move.end_file)] = captured_piece;

    uint64_t start_bit = 1ULL << (move.start_rank * 8 + move.start_file);
    uint64_t end_bit = 1ULL << (move.end_rank * 8 + move.end_file);

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
        // Generic undo put the promoted piece back on the start square;
        // turn it into a pawn again (capture already restored on the end square).
        Color promoColor = piece.getColor();

        switch (piece.getType()) {
            case PieceType::KNIGHT:
                (promoColor == Color::WHITE ? white_knights : black_knights) &= ~start_bit;
                break;
            case PieceType::BISHOP:
                (promoColor == Color::WHITE ? white_bishops : black_bishops) &= ~start_bit;
                break;
            case PieceType::ROOK:
                (promoColor == Color::WHITE ? white_rooks : black_rooks) &= ~start_bit;
                break;
            default: // QUEEN
                (promoColor == Color::WHITE ? white_queens : black_queens) &= ~start_bit;
                break;
        }

        if (promoColor == Color::WHITE) {
            white_pawns |= start_bit;
        } else {
            black_pawns |= start_bit;
        }

        board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)] = Piece(PieceType::PAWN, promoColor);
    }

    en_passant_stack.pop();
    int ep;
    if (!en_passant_stack.empty())
        ep = en_passant_stack.top();
    else
        ep = 64;

    // Keep the member in sync with the stack (used to only pop the stack).
    en_passant_square = ep;

    if (move.end_rank * 8 + move.end_file == ep &&
        board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)].getType() == PieceType::PAWN) {

        int capture_rank = (board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)].getColor() == Color::WHITE)
                            ? move.end_rank - 1
                            : move.end_rank + 1;
        int capture_file = move.end_file;

        board[static_cast<size_t>(capture_rank)][static_cast<size_t>(capture_file)] = Piece(PieceType::PAWN, 
            board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)].getColor() == Color::WHITE ? Color::BLACK : Color::WHITE);

        uint64_t capture_bit = 1ULL << (capture_rank * 8 + capture_file);
        if (board[static_cast<size_t>(move.start_rank)][static_cast<size_t>(move.start_file)].getColor() == Color::WHITE) {
            black_pawns |= capture_bit;
        } else {
            white_pawns |= capture_bit;
        }
    }

    turn_index--;
    all_white_pieces = white_pawns | white_knights | white_rooks | white_bishops | white_queens | white_kings;
    all_black_pieces = black_pawns | black_knights | black_rooks | black_bishops | black_queens | black_kings;
    all_pieces = all_white_pieces | all_black_pieces;
}


void Board::display() {
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
    uint64_t empty = ~all_pieces;
    int direction = (color == Color::WHITE) ? 8 : -8;

    // Last-rank pawn moves expand into four promotion choices.
    auto push_pawn_move = [&moves](int from_square, int to_square) {
        const int to_rank = to_square / 8;
        if (to_rank == 0 || to_rank == 7) {
            for (PieceType promo :
                 {PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP,
                  PieceType::KNIGHT}) {
                moves.push_back({from_square % 8, from_square / 8,
                                 to_square % 8, to_square / 8, promo});
            }
        } else {
            moves.push_back({from_square % 8, from_square / 8,
                             to_square % 8, to_square / 8});
        }
    };

    uint64_t single_pushes = (color == Color::WHITE)
                                ? ((pawns << 8) & empty)
                                : ((pawns >> 8) & empty);

    while (single_pushes) {
        int to_square = __builtin_ctzll(single_pushes);
        single_pushes &= (single_pushes - 1);

        int from_square = to_square - direction;
        push_pawn_move(from_square, to_square);
    }

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

    constexpr uint64_t FILE_H = 0x0101010101010101ULL; 
    constexpr uint64_t FILE_A = 0x8080808080808080ULL;

    uint64_t left_capt = (color == Color::WHITE)
                            ? ((pawns << 9) & ~FILE_H & all_black_pieces)
                            : ((pawns >> 7) & ~FILE_H & all_white_pieces);

    while (left_capt) {
        int to_square = __builtin_ctzll(left_capt);
        left_capt &= (left_capt - 1);

        int from_square = to_square - (direction + 1);
        push_pawn_move(from_square, to_square);
    }

    uint64_t right_capt = (color == Color::WHITE)
                            ? ((pawns << 7) & ~FILE_A & all_black_pieces)
                            : ((pawns >> 9) & ~FILE_A & all_white_pieces);

    while (right_capt) {
        int to_square = __builtin_ctzll(right_capt);
        right_capt &= (right_capt - 1);

        int from_square = to_square - (direction - 1);
        push_pawn_move(from_square, to_square);
    }

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
    // Use KNIGHT_ATTACKS instead of generating on the fly.

    while (knights) {
        int from_square = __builtin_ctzll(knights); 
        knights &= knights - 1; // pop the knight bit

        uint64_t attack_mask = KNIGHT_ATTACKS[from_square];

        uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;
        uint64_t valid_moves = attack_mask & ~friendlies;

        while (valid_moves) {
            int to_square = __builtin_ctzll(valid_moves);
            valid_moves &= (valid_moves - 1);

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

        uint64_t attack_mask = generate_rook_attack_mask(from_bit, all_pieces);

        uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;
        uint64_t valid_moves = attack_mask & ~friendlies;

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
        int from_sq = __builtin_ctzll(bishops);   // index [0..63]

        uint64_t occupancy = all_white_pieces | all_black_pieces;

        uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;

        uint64_t attack_mask = magicAttacks.Bishop(occupancy, from_sq);

        uint64_t valid_moves = attack_mask & ~friendlies;

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

        bishops &= (bishops - 1);
    }
}


void Board::generate_queen_moves(uint64_t queens, Color color, std::vector<Move> & moves) 
{
    while (queens) {
        int from_sq = __builtin_ctzll(queens);


        uint64_t occupancy  = all_white_pieces | all_black_pieces;
        uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;

        uint64_t attack_mask = magicAttacks.Queen(occupancy, from_sq);

        uint64_t valid_moves = attack_mask & ~friendlies;

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
    for (int sq = 0; sq < 64; sq++) {
        KNIGHT_ATTACKS[sq] = 0ULL;
        KING_ATTACKS[sq] = 0ULL;

        static const int knightOffsets[8][2] = {
            { 2,  1}, { 2, -1}, {-2,  1}, {-2, -1},
            { 1,  2}, { 1, -2}, {-1,  2}, {-1, -2}
        };

        static const int kingOffsets[8][2] = {
            { 1, 0}, {-1, 0}, {0,  1}, {0, -1},
            { 1, 1}, { 1, -1}, {-1, 1}, {-1, -1}
        };

        for (auto & off : knightOffsets) {
            int target = shift_square(sq, off[0], off[1]);
            if (target != -1) {
                KNIGHT_ATTACKS[sq] |= one_square(target);
            }
        }

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
        return;
    }

    if (color == Color::WHITE) {
    
        if (rights & 0b0001)  // White kingside
        {
            bool f1_empty = !(all_pieces & (1ULL << 5));
            bool g1_empty = !(all_pieces & (1ULL << 6));
            bool f1_safe = !(black_attacks & (1ULL << 5));
            bool g1_safe = !(black_attacks & (1ULL << 6));

            if (f1_empty && g1_empty && f1_safe && g1_safe) {
                // Rook ride-along is handled inside make_move.
                moves.push_back(Move{
                    4 % 8, 4 / 8,   // from e1
                    6 % 8, 6 / 8    // to   g1
                });
            }
        }

        // White queenside (e1 -> c1)
        if (rights & 0b0010)  // White can castle queenside
        {
            // b1/c1/d1 empty
            bool b1_empty = !(all_pieces & (1ULL << 1));
            bool c1_empty = !(all_pieces & (1ULL << 2));
            bool d1_empty = !(all_pieces & (1ULL << 3));
            // king may not pass through or land in check
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
        // Black kingside (e8 -> g8)
        if (rights & 0b0100)  // Black can castle kingside
        {
            bool f8_empty = !(all_pieces & (1ULL << 61));
            bool g8_empty = !(all_pieces & (1ULL << 62));
            bool f8_safe = !(white_attacks & (1ULL << 61));
            bool g8_safe = !(white_attacks & (1ULL << 62));

            if (f8_empty && g8_empty && f8_safe && g8_safe) {
                moves.push_back(Move{
                    60 % 8, 60 / 8,  // from e8
                    62 % 8, 62 / 8   // to   g8
                });
            }
        }

        // Black queenside (e8 -> c8)
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
    uint64_t king_pos = 1ULL << (king_row * 8 + king_col);  // King's position as a bitboard

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
    // magic_bits wants occupancy + square index
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

    // Pseudo-legal: piece rules only, ignore pins/checks.

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
    // Not in check, no legal moves. generate_legal_moves restores the board.
    if (is_check(color))
        return false;

    return generate_legal_moves(color).empty();
}


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
uint16_t Board::getCastlingRights() const {
    return castling_rights;
}


void Board::set_turn_index(size_t index) {
    turn_index = index;
}

uint8_t Board::getEnPassantSquare() const {
    if (en_passant_square >= 0 && en_passant_square < 64) {
        return static_cast<uint8_t>(en_passant_square);
    }

    return 0xFF;
}


static const int KING_RAYS[8] = {8, -8, 1, -1, 9, 7, -7, -9};

void Board::compute_attack_masks() {

    white_attacks = 0ULL;
    black_attacks = 0ULL;

    uint64_t w_p = white_pawns;
    white_attacks |= ((w_p << 7) & ~0x8080808080808080ULL);
    white_attacks |= ((w_p << 9) & ~0x0101010101010101ULL);

    uint64_t b_p = black_pawns;
    black_attacks |= ((b_p >> 7) & ~0x0101010101010101ULL);
    black_attacks |= ((b_p >> 9) & ~0x8080808080808080ULL);

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

    if (white_kings) {
        int sq = __builtin_ctzll(white_kings);
        white_attacks |= KING_ATTACKS[sq];
    }
    if (black_kings) {
        int sq = __builtin_ctzll(black_kings);
        black_attacks |= KING_ATTACKS[sq];
    }

    uint64_t w_rq = white_rooks | white_queens;
    while (w_rq) {
        int sq = __builtin_ctzll(w_rq);
        white_attacks |= generate_rook_attack_mask(1ULL << sq, all_pieces);
        w_rq &= w_rq - 1;
    }

    uint64_t b_rq = black_rooks | black_queens;
    while (b_rq) {
        int sq = __builtin_ctzll(b_rq);
        black_attacks |= generate_rook_attack_mask(1ULL << sq, all_pieces);
        b_rq &= b_rq - 1;
    }

    uint64_t w_bq = white_bishops | white_queens;
    while (w_bq) {
        int sq = __builtin_ctzll(w_bq);
        white_attacks |= generate_bishop_attack_mask(1ULL << sq);
        w_bq &= w_bq - 1;
    }

    uint64_t b_bq = black_bishops | black_queens;
    while (b_bq) {
        int sq = __builtin_ctzll(b_bq);
        black_attacks |= generate_bishop_attack_mask(1ULL << sq);
        b_bq &= b_bq - 1;
    }
}


void Board::detect_pins(Color us, int king_sq) {
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
            int prev = sq - d;
            if ((d == 1 || d == -1 || d == 9 || d == 7 || d == -7 || d == -9)) {
                int file_diff = abs((sq % 8) - (prev % 8));
                if (file_diff != 1) break;
            }

            uint64_t bit = 1ULL << sq;
            if (all_pieces & bit) {
                if (!found_own) {
                    if (own_pieces & bit) {
                        found_own = true;
                        own_piece_sq = sq;
                    } else {
                        break;
                    }
                } else {
                    bool rook_dir = (d == 8 || d == -8 || d == 1 || d == -1);
                    bool bishop_dir = (d == 9 || d == 7 || d == -7 || d == -9);

                    uint64_t bit2 = 1ULL << sq;
                    bool is_sliding_attacker = false;

                    // Look for an opposing slider behind the pinned piece
                    if (rook_dir && ((opp_rooks | opp_queens) & bit2)) {
                        is_sliding_attacker = true;
                    } else if (bishop_dir && ((opp_bishops | opp_queens) & bit2)) {
                        is_sliding_attacker = true;
                    }

                    if (is_sliding_attacker && own_piece_sq != -1) {
                        pinned_pieces |= (1ULL << own_piece_sq);

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

// Recompute attack masks
    compute_attack_masks();

    // Find our king for pin detection
    uint64_t king_bb = (color == Color::WHITE) ? white_kings : black_kings;

    if (king_bb == 0ULL) {
        // No king detected, SOMETHING HAS GONE TERRIBLY WRONG
        throw Error("No King");
    }
    int king_sq = __builtin_ctzll(king_bb); //get index of lsb

    // fills pinned_pieces / pin_line
    detect_pins(color, king_sq);

    std::vector<Move> pseudo_moves = generate_pseudo_legal_moves(color);

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
    // Opponent attacks (king can't step into these)
    uint64_t enemy_attacks = (color == Color::WHITE) ? black_attacks : white_attacks;

    for (const auto & move : pseudo_moves)
    {
        int from_sq = move.start_rank * 8 + move.start_file;
        int to_sq   = move.end_rank   * 8 + move.end_file;

        // Pinned pieces may only move along the pin line
        if (is_pinned_piece(from_sq)) {
            uint64_t to_bit = (1ULL << to_sq);
            if ((pin_line[from_sq] & to_bit) == 0ULL) {
                // off the pin line
                continue;
            }
        }

        // King can't walk into check
        Piece from_piece = getPiece(move.start_rank, move.start_file);
        if (from_piece.getType() == PieceType::KING) {
            uint64_t to_bit = (1ULL << to_sq);
            if (to_bit & enemy_attacks) {
                continue;
            }
        }

        Piece captured_piece = getPiece(move.end_rank, move.end_file);
        make_move(move);

        // Still in check after the move => illegal
        if (is_check(color)) {
            undo_move(move, captured_piece);
            continue;
        }

        undo_move(move, captured_piece);
        legal_moves.push_back(move);
    }
}


void Board::print_position() {

    string toPrint = "";
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
    
    cout << toPrint << endl;
}


Color Board::get_color() const {
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

    uint64_t attacker_pawns = (attacker_color == Color::WHITE) ? white_pawns : black_pawns;
    uint64_t pawn_attacks = (attacker_color == Color::WHITE) ?
                            ((attacker_pawns << 7) & ~FILE_H) |
                            ((attacker_pawns << 9) & ~FILE_A) :
                            ((attacker_pawns >> 7) & ~FILE_H) |
                            ((attacker_pawns >> 9) & ~FILE_A);
    if (pawn_attacks & square_bit)
        return true;

    uint64_t attacker_knights = (attacker_color == Color::WHITE) ? white_knights : black_knights;
    uint64_t knight_attacks = KNIGHT_ATTACKS[square];
    if (knight_attacks & attacker_knights)
        return true;

    uint64_t attacker_bishops = (attacker_color == Color::WHITE) ? white_bishops : black_bishops;
    uint64_t attacker_queens_bishop = (attacker_color == Color::WHITE) ? white_queens : black_queens;
    uint64_t bishop_attacks = magicAttacks.Bishop(all_pieces, square);
    if ((bishop_attacks & attacker_bishops) || (bishop_attacks & attacker_queens_bishop))
        return true;

    uint64_t attacker_rooks = (attacker_color == Color::WHITE) ? white_rooks : black_rooks;
    uint64_t attacker_queens_rook = (attacker_color == Color::WHITE) ? white_queens : black_queens;
    uint64_t rook_attacks = magicAttacks.Rook(all_pieces, square);
    if ((rook_attacks & attacker_rooks) || (rook_attacks & attacker_queens_rook))
        return true;

    uint64_t attacker_kings = (attacker_color == Color::WHITE) ? white_kings : black_kings;
    uint64_t king_attacks = KING_ATTACKS[square];
    if (king_attacks & attacker_kings)
        return true;

    return false;
}

// Find pieces checking the king
vector<int> Board::find_checking_pieces(int king_sq, Color attacker_color) const {
    vector<int> checkers;
    uint64_t king_bit = 1ULL << king_sq;

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

    uint64_t attacker_knights = (attacker_color == Color::WHITE) ? white_knights : black_knights;
    uint64_t knight_attacks = KNIGHT_ATTACKS[king_sq] & attacker_knights;
    while (knight_attacks) {
        int checker_sq = __builtin_ctzll(knight_attacks);
        checkers.push_back(checker_sq);
        knight_attacks &= knight_attacks - 1;
    }

    uint64_t attacker_bishops = (attacker_color == Color::WHITE) ? white_bishops : black_bishops;
    uint64_t attacker_queens_bishop = (attacker_color == Color::WHITE) ? white_queens : black_queens;
    uint64_t bishop_attacks = magicAttacks.Bishop(all_pieces, king_sq);
    uint64_t bishop_checks = (bishop_attacks & attacker_bishops) | (bishop_attacks & attacker_queens_bishop);
    while (bishop_checks) {
        int checker_sq = __builtin_ctzll(bishop_checks);
        checkers.push_back(checker_sq);
        bishop_checks &= bishop_checks - 1;
    }

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

// Direction between two squares (for blockers)
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

// Captures of the checking piece
void Board::generate_captures(Color color, int checker_sq, vector<Move> & moves) {
    uint64_t target_bit = 1ULL << checker_sq;


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

    uint64_t knights = (color == Color::WHITE) ? white_knights : black_knights;
    uint64_t knight_attacks = KNIGHT_ATTACKS[checker_sq] & knights;
    while (knight_attacks) {
        int from_sq = __builtin_ctzll(knight_attacks);
        knight_attacks &= knight_attacks - 1;
        moves.emplace_back(Move{from_sq % 8, from_sq / 8, checker_sq % 8, checker_sq / 8});
    }

    uint64_t bishops = (color == Color::WHITE) ? white_bishops : black_bishops;
    uint64_t queens_bishop = (color == Color::WHITE) ? white_queens : black_queens;
    uint64_t bishop_attacks = magicAttacks.Bishop(all_pieces, checker_sq);
    uint64_t capturing_bishops = (bishop_attacks & bishops) | (bishop_attacks & queens_bishop);
    while (capturing_bishops) {
        int from_sq = __builtin_ctzll(capturing_bishops);
        capturing_bishops &= capturing_bishops - 1;
        moves.emplace_back(Move{from_sq % 8, from_sq / 8, checker_sq % 8, checker_sq / 8});
    }

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


void Board::generate_blocks(Color color, int checker_sq, vector<Move> & moves) {

    int king_sq = (color == Color::WHITE) ? __builtin_ctzll(white_kings) : __builtin_ctzll(black_kings);
    uint64_t between = get_between_squares(king_sq, checker_sq);
    if (between == 0ULL)
        return; // Not a sliding piece or adjacent

    for (int sq = 0; sq < 64; ++sq) {
        if (between & (1ULL << sq)) {

            uint64_t pawns = (color == Color::WHITE) ? white_pawns : black_pawns;
            if (color == Color::WHITE) {
                if (sq >= 8 && !(all_pieces & (1ULL << sq))) {
                    uint64_t pawn_from = (1ULL << (sq - 8)) & pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
                if (sq >= 16 && (sq % 8) == (sq - 8) % 8 &&
                    !(all_pieces & (1ULL << sq)) && !(all_pieces & (1ULL << (sq - 8)))) {
                    uint64_t pawn_from = (1ULL << (sq - 16)) & pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
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
                if (sq <= 55 && !(all_pieces & (1ULL << sq))) {
                    uint64_t pawn_from = (1ULL << (sq + 8)) & black_pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
                if (sq <= 47 && (sq % 8) == (sq + 8) % 8 &&
                    !(all_pieces & (1ULL << sq)) && !(all_pieces & (1ULL << (sq + 8)))) {
                    uint64_t pawn_from = (1ULL << (sq + 16)) & black_pawns;
                    while (pawn_from) {
                        int from_sq = __builtin_ctzll(pawn_from);
                        pawn_from &= pawn_from - 1;
                        moves.emplace_back(Move{from_sq % 8, from_sq / 8, sq % 8, sq / 8});
                    }
                }
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


// King moves that don't walk into check
void Board::generate_king_moves(uint64_t kings, Color color, vector<Move> & moves, bool double_check) {
    if (kings == 0ULL) return;

    int king_sq = __builtin_ctzll(kings); // Assuming only one king
    uint64_t attack_mask = KING_ATTACKS[king_sq];
    
    uint64_t friendlies = (color == Color::WHITE) ? all_white_pieces : all_black_pieces;
    uint64_t valid_moves = attack_mask & ~friendlies;

    Color opponent_color = (color == Color::WHITE) ? Color::BLACK : Color::WHITE;
    uint64_t enemy_attacks = (opponent_color == Color::WHITE) ? white_attacks : black_attacks;
    valid_moves &= ~enemy_attacks;

    while (valid_moves) {
        int to_sq = __builtin_ctzll(valid_moves);
        valid_moves &= valid_moves - 1;

        moves.emplace_back(Move{
            king_sq % 8, king_sq / 8,
            to_sq % 8,   to_sq / 8
        });
    }

    if (double_check) {
        // No castling out of double check
        return;
    }

    uint16_t rights = getCastlingRights();
    if (color == Color::WHITE) {
        // Kingside
        if (rights & 0b0001) { // White can castle kingside
            if (!(all_pieces & ((1ULL << 5) | (1ULL << 6)))) {
                if (!is_square_attacked(5, opponent_color) && !is_square_attacked(6, opponent_color)) {
                    moves.emplace_back(Move{4, 0, 6, 0}); // e1 to g1
                }
            }
        }

        // Queenside
        if (rights & 0b0010) { // White can castle queenside
            if (!(all_pieces & ((1ULL << 1) | (1ULL << 2) | (1ULL << 3)))) {
                if (!is_square_attacked(2, opponent_color) && !is_square_attacked(3, opponent_color)) {
                    moves.emplace_back(Move{4, 0, 2, 0}); // e1 to c1
                }
            }
        }
    }
    else { // Color::BLACK
        // Kingside
        if (rights & 0b0100) { // Black can castle kingside
            if (!(all_pieces & ((1ULL << 61) | (1ULL << 62)))) {
                if (!is_square_attacked(61, opponent_color) && !is_square_attacked(62, opponent_color)) {
                    moves.emplace_back(Move{60, 7, 62, 7}); // e8 to g8
                }
            }
        }

        // Queenside
        if (rights & 0b1000) { // Black can castle queenside
            if (!(all_pieces & ((1ULL << 57) | (1ULL << 58) | (1ULL << 59)))) {
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


bool Board::set_from_fen(const std::string& fen)
{
    std::istringstream ss(fen);

    std::string piece_placement;
    std::string active_color;
    std::string castling;
    std::string en_passant;
    int halfmove_clock;
    int fullmove_number;

    if (!(ss >> piece_placement
             >> active_color
             >> castling
             >> en_passant
             >> halfmove_clock
             >> fullmove_number))
    {
        return false;
    }

    clear();


    int rank = 7;
    int file = 0;

    for (char c : piece_placement)
    {
        if (c == '/')
        {
            if (file != 8 || rank == 0)
                return false;

            --rank;
            file = 0;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)))
        {
            int empty_count = c - '0';

            if (empty_count < 1 || empty_count > 8)
                return false;

            file += empty_count;

            if (file > 8)
                return false;

            continue;
        }

        if (file >= 8)
            return false;

        Color color =
            std::isupper(static_cast<unsigned char>(c))
                ? Color::WHITE
                : Color::BLACK;

        PieceType type;

        switch (std::tolower(static_cast<unsigned char>(c)))
        {
            case 'p': type = PieceType::PAWN;   break;
            case 'n': type = PieceType::KNIGHT; break;
            case 'b': type = PieceType::BISHOP; break;
            case 'r': type = PieceType::ROOK;   break;
            case 'q': type = PieceType::QUEEN;  break;
            case 'k': type = PieceType::KING;   break;
            default:
                return false;
        }

        int square = rank * 8 + file;
        uint64_t bit = 1ULL << square;

        board[static_cast<size_t>(rank)][static_cast<size_t>(file)] =
            Piece(type, color, static_cast<size_t>(square));

        if (color == Color::WHITE)
        {
            switch (type)
            {
                case PieceType::PAWN:   white_pawns   |= bit; break;
                case PieceType::KNIGHT: white_knights |= bit; break;
                case PieceType::BISHOP: white_bishops |= bit; break;
                case PieceType::ROOK:   white_rooks   |= bit; break;
                case PieceType::QUEEN:  white_queens  |= bit; break;
                case PieceType::KING:   white_kings   |= bit; break;
                default: break;
            }
        }
        else
        {
            switch (type)
            {
                case PieceType::PAWN:   black_pawns   |= bit; break;
                case PieceType::KNIGHT: black_knights |= bit; break;
                case PieceType::BISHOP: black_bishops |= bit; break;
                case PieceType::ROOK:   black_rooks   |= bit; break;
                case PieceType::QUEEN:  black_queens  |= bit; break;
                case PieceType::KING:   black_kings   |= bit; break;
                default: break;
            }
        }

        ++file;
    }

    if (rank != 0 || file != 8)
        return false;


    all_white_pieces =
        white_pawns |
        white_knights |
        white_bishops |
        white_rooks |
        white_queens |
        white_kings;

    all_black_pieces =
        black_pawns |
        black_knights |
        black_bishops |
        black_rooks |
        black_queens |
        black_kings;

    all_pieces = all_white_pieces | all_black_pieces;


    if (fullmove_number < 1)
        return false;

    turn_index = static_cast<size_t>(2 * (fullmove_number - 1));

    if (active_color == "b")
    {
        ++turn_index;
    }
    else if (active_color != "w")
    {
        return false;
    }


    if (castling != "-")
    {
        for (char c : castling)
        {
            switch (c)
            {
                case 'K':
                    castling_rights |= WHITE_KINGSIDE;
                    break;

                case 'Q':
                    castling_rights |= WHITE_QUEENSIDE;
                    break;

                case 'k':
                    castling_rights |= BLACK_KINGSIDE;
                    break;

                case 'q':
                    castling_rights |= BLACK_QUEENSIDE;
                    break;

                default:
                    return false;
            }
        }
    }


    if (en_passant == "-")
    {
        en_passant_square = 64;
    }
    else
    {
        if (en_passant.size() != 2)
            return false;

        int ep_file = en_passant[0] - 'a';
        int ep_rank = en_passant[1] - '1';

        if (ep_file < 0 || ep_file >= 8 ||
            ep_rank < 0 || ep_rank >= 8)
        {
            return false;
        }

        en_passant_square = ep_rank * 8 + ep_file;
    }

    // generate_pawn_moves reads EP from the stack; seed it after clear().
    en_passant_stack.push(en_passant_square);

    compute_attack_masks();

    return true;
}


std::string Board::to_fen() const
{
    std::ostringstream fen;


    for (int rank = 7; rank >= 0; --rank)
    {
        int empty_count = 0;

        for (int file = 0; file < 8; ++file)
        {
            const Piece& piece =
                board[static_cast<size_t>(rank)][static_cast<size_t>(file)];

            if (piece.getType() == PieceType::NONE)
            {
                ++empty_count;
                continue;
            }

            if (empty_count > 0)
            {
                fen << empty_count;
                empty_count = 0;
            }

            char c;

            switch (piece.getType())
            {
                case PieceType::PAWN:   c = 'p'; break;
                case PieceType::KNIGHT: c = 'n'; break;
                case PieceType::BISHOP: c = 'b'; break;
                case PieceType::ROOK:   c = 'r'; break;
                case PieceType::QUEEN:  c = 'q'; break;
                case PieceType::KING:   c = 'k'; break;
                default:
                    c = '?';
                    break;
            }

            if (piece.getColor() == Color::WHITE)
            {
                c = static_cast<char>(
                    std::toupper(static_cast<unsigned char>(c))
                );
            }

            fen << c;
        }

        if (empty_count > 0)
            fen << empty_count;

        if (rank != 0)
            fen << '/';
    }


    fen << ' '
        << ((turn_index % 2 == 0) ? 'w' : 'b');


    fen << ' ';

    uint16_t rights = getCastlingRights();

    if (rights == 0)
    {
        fen << '-';
    }
    else
    {
        if (rights & (1 << 0)) fen << 'K';
        if (rights & (1 << 1)) fen << 'Q';
        if (rights & (1 << 2)) fen << 'k';
        if (rights & (1 << 3)) fen << 'q';
    }


    fen << ' ';

    if (en_passant_square == 64)
    {
        fen << '-';
    }
    else
    {
        int ep_file = en_passant_square % 8;
        int ep_rank = en_passant_square / 8;

        fen << static_cast<char>('a' + ep_file);
        fen << static_cast<char>('1' + ep_rank);
    }


    // You don't currently store this.
    fen << " 0";


    fen << ' ' << (turn_index / 2 + 1);

    return fen.str();
}