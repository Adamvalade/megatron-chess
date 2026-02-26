#ifndef TESTS_H
#define TESTS_H

#include "board.h"
#include "engine.h"
#include "game.h"
#include "Piece.h"
#include "magic.h"
#include <set>
#include <string>
#include <iostream>
#include <vector>
#include <cassert>

using namespace std;

void test_five_moves();
void test_generate_all_moves_depth3();
void test_en_passant();
void test_checkmate();
void test_five_perfs();
void test_print_position();
void test_bitboards();
void print_all_fen_positions();
void test_bishop_captures();
void move_generation_test_helper(int depth);
void generate_magic_nums();
void testRookAttacks(int sq);
void test_bits();
uint64_t moveGenerationTest(int depth, Color c, Board * b);


void run_tests() {
    move_generation_test_helper(5);
    // generate_magic_nums();
}


void generate_magic_nums() {
    using namespace magic_bits;

    // Create a generator for bishops
    Attacks::Generator<Attacks::PieceType::BISHOP> bishop_gen;

    // Create a generator for rooks
    Attacks::Generator<Attacks::PieceType::ROOK> rook_gen;

    std::cout << "Bishop Magic Numbers:" << std::endl;
    for (int i = 0; i < 64; ++i) {
        std::cout << "Square " << i << ": 0x" << std::hex << bishop_gen.magics()[static_cast<size_t>(i)] << std::dec << std::endl;
    }

    std::cout << "\nRook Magic Numbers:" << std::endl;
    for (int i = 0; i < 64; ++i) {
        std::cout << "Square " << i << ": 0x" << std::hex << rook_gen.magics()[static_cast<size_t>(i)] << std::dec << std::endl;
    }
}

void test_bishop_captures() {

    Board * b = new Board;
    
    Move m1 = Move{3, 1, 3, 3,};
    Move m2 = Move{2, 6, 2, 4};
    Move m3 = Move{2, 0, 3, 1};
    Move m4 = Move{3, 7, 0, 4};

    b->make_move(m1);
    b->make_move(m2);
    b->make_move(m3);
    b->make_move(m4);

    b->display();

    vector<Move> moves = b->generate_all_moves(Color::WHITE);

    delete b;

}


void test_bits() {
    magic_bits::Attacks magicAttacks;
    // Example: Generate attack mask for a rook on square 36 (d5)
    uint64_t all_pieces = 0ULL;  // Empty board
    uint64_t rook_attacks = magicAttacks.Rook(all_pieces, 36);

    std::cout << "Rook attacks: " << std::hex << rook_attacks << std::endl;

}


void print_all_fen_positions() {

    Board * b = new Board;
    vector<Move> first = b->generate_all_moves(Color::WHITE);

    for (const auto & i : first) {

        Piece p1 = b->get_piece_at(i.end_rank, i.end_file);
        b->make_move(i);
        vector<Move> second = b->generate_all_moves(Color::BLACK);

        for (const auto & j : second) {

            Piece p2 = b->get_piece_at(j.end_rank, j.end_file);
            b->make_move(j);
            vector<Move> third = b->generate_all_moves(Color::WHITE);

            for (const auto & k : third) {

                Piece p3 = b->get_piece_at(k.end_rank, k.end_file);
                b->make_move(k);
                vector<Move> fourth = b->generate_all_moves(Color::BLACK);

                for (const auto & m : fourth) {

                    Piece p4 = b->get_piece_at(m.end_rank, m.end_file);
                    b->make_move(m);

                    if (b->is_checkmate(Color::WHITE) || b->is_checkmate(Color::BLACK)) {
                        b->print_position();
                    } // print line if it ends in checkmate

                    vector<Move> fifth = b->generate_all_moves(Color::WHITE);

                    for (const auto & c : fifth) {
                        Piece p5 = b->get_piece_at(c.end_rank, c.end_file);
                        b->make_move(c);
                        b->print_position();
                        b->undo_move(c, p5);

                    } // for c

                    b->undo_move(m, p4);
                } // for m

                b->undo_move(k, p3);
            } // for k

            b->undo_move(j, p2);
        } // for j

        b->undo_move(i, p1);
    } // for i

    delete b;
} // test 5 perfs


void move_generation_test_helper(int depth) {
    Board * b = new Board;
    cout << moveGenerationTest(depth, Color::WHITE, b) << "\n\n";
    delete b;

}

void test_en_passant() {

    cout << "Testing for en passant\n";

    Board * b = new Board;

    Move m1 = {4, 1, 4, 3}; // e2e4
    Move m2 = {3, 6, 3, 4}; // d7d5
    Move m3 = {4, 3, 4, 4}; // e4e5
    Move m4 = {5, 6, 5, 4}; // f7f5

    // en passant should be available next move

    b->make_move(m1);
    b->make_move(m2);
    b->make_move(m3);
    b->make_move(m4);

    b->display();

    vector<Move> moves = b->generate_all_moves(Color::WHITE);

    cout << "\nlooking for move e5f6\n";

    bool found = false;

    for (const auto & i : moves) {
        if (i.end_file == 5 && i.end_rank == 5
            && i.start_rank == 4 && i.start_file == 4) {
                found = true;
                b->make_move(i);
        }
    }

    if (found) {
        cout << "\ntest PASSED\n\n";
        b->display();
    }
    else {
        cout << "\ntest FAILED\n"
             << "Moves found:\n";

        for (const auto i : moves) {
            cout << static_cast<char>(65 + i.start_file) << i.start_rank + 1 << " to "
                 << static_cast<char>(65 + i.end_file) << i.end_rank + 1 << "\n";
        }
    }

    delete b;
}




void test_checkmate() {

    Board * b = new Board;

    cout << "Testing checkmate\n";

    Move m1 = Move{4, 1, 4, 3}; // e2e4
    Move m2 = Move{5, 0, 2, 3}; // f1c4
    Move m3 = Move{0, 6, 0, 5}; // a7a6
    Move m4 = Move{3, 0, 5, 2}; // d1f3
    Move m5 = Move{6, 7, 4, 6}; // g8e7
    Move m6 = Move{5, 2, 5, 6}; // f3f7

    b->make_move(m1);
    b->make_move(m2);
    b->make_move(m3);
    b->make_move(m4);
    b->make_move(m5);
    b->make_move(m6);

    b->display();

    vector<Move> moves = b->generate_all_moves(Color::BLACK);
    size_t num_moves = moves.size();


    bool passed = b->is_checkmate(Color::BLACK);
    passed &= (num_moves == 0);

    if (passed) 
        cout << "\n\n\ntest PASSED\n\n\n";
    else
        cout << "\n\n\ntest FAILED\n\n\n";

    return;

}

void test_five_moves() {

    Board * b = new Board;

    size_t white_first_moves_count = 0;
    size_t black_first_moves_count = 0;
    size_t white_second_moves_count = 0;
    size_t black_second_moves_count = 0;
    size_t white_third_moves_count = 0;
    size_t checkmate_count = 0;

    // First move by White
    vector<Move> white_moves = b->generate_all_moves(Color::WHITE);
    white_first_moves_count = white_moves.size();

    for (const auto &white_move : white_moves) {
        Piece captured_white = b->get_piece_at(white_move.end_rank, white_move.end_file);
        b->make_move(white_move);

        vector<Move> black_moves = b->generate_all_moves(Color::BLACK);
        black_first_moves_count += black_moves.size();

        for (const auto &black_move : black_moves) {
            Piece captured_black = b->get_piece_at(black_move.end_rank, black_move.end_file);
            b->make_move(black_move);

            vector<Move> second_white_moves = b->generate_all_moves(Color::WHITE);
            white_second_moves_count += second_white_moves.size();

            for (const auto &second_white_move : second_white_moves) {
                Piece captured_second_white = b->get_piece_at(second_white_move.end_rank, second_white_move.end_file);
                b->make_move(second_white_move);

                vector<Move> second_black_moves = b->generate_all_moves(Color::BLACK);
                black_second_moves_count += second_black_moves.size();

                for (const auto &second_black_move : second_black_moves) {
                    Piece captured_second_black = b->get_piece_at(second_black_move.end_rank, second_black_move.end_file);
                    b->make_move(second_black_move);

                    vector<Move> third_white_moves = b->generate_all_moves(Color::WHITE);
                    white_third_moves_count += third_white_moves.size();

                    for (const auto &third_white_move : third_white_moves) {
                        Piece captured_third_white = b->get_piece_at(third_white_move.end_rank, third_white_move.end_file);
                        b->make_move(third_white_move);

                        // Check for checkmate after White's third move (5th Ply)
                        if (b->generate_all_moves(Color::BLACK).empty() && b->is_check(Color::BLACK)) {
                            checkmate_count++;
                        }

                        // Undo White's third move
                        b->undo_move(third_white_move, captured_third_white);
                    }

                    // Undo Black's second move
                    b->undo_move(second_black_move, captured_second_black);
                }

                // Undo White's second move
                b->undo_move(second_white_move, captured_second_white);
            }

            // Undo Black's first move
            b->undo_move(black_move, captured_black);
        }

        // Undo White's first move
        b->undo_move(white_move, captured_white);
    }

    std::cout << "White's first moves: " << white_first_moves_count << "\n";
    std::cout << "Black's first moves: " << black_first_moves_count << "\n";
    std::cout << "White's second moves: " << white_second_moves_count << "\n";
    std::cout << "Black's second moves: " << black_second_moves_count << "\n";
    std::cout << "White's third moves: " << white_third_moves_count << "\n";
    std::cout << "Checkmates found: " << checkmate_count << "\n";

    delete b;
}


void test_five_perfs() {

    Board * b = new Board;
    vector<Move> first = b->generate_all_moves(Color::WHITE);

    int num_lines = 0;

    for (const auto & i : first) {

        Piece p1 = b->get_piece_at(i.end_rank, i.end_file);
        b->make_move(i);
        vector<Move> second = b->generate_all_moves(Color::BLACK);

        for (const auto & j : second) {

            Piece p2 = b->get_piece_at(j.end_rank, j.end_file);
            b->make_move(j);
            vector<Move> third = b->generate_all_moves(Color::WHITE);

            for (const auto & k : third) {

                Piece p3 = b->get_piece_at(k.end_rank, k.end_file);
                b->make_move(k);
                vector<Move> fourth = b->generate_all_moves(Color::BLACK);

                for (const auto & m : fourth) {

                    Piece p4 = b->get_piece_at(m.end_rank, m.end_file);
                    b->make_move(m);

                    if (b->is_checkmate(Color::WHITE) || b->is_checkmate(Color::BLACK)) {
                        num_lines++;
                        cout << num_lines << ": "
                             << static_cast<char>(i.start_file + 65) << i.start_rank + 1 << " to "
                             << static_cast<char>(i.end_file + 65) << i.end_rank + 1 << ", "
                             << static_cast<char>(j.start_file + 65) << j.start_rank + 1 << " to "
                             << static_cast<char>(j.end_file + 65) << j.end_rank + 1 << ", "
                             << static_cast<char>(k.start_file + 65) << k.start_rank + 1 << " to "
                             << static_cast<char>(k.end_file + 65) << k.end_rank + 1 << ", "
                             << static_cast<char>(m.start_file + 65) << m.start_rank + 1 << " to "
                             << static_cast<char>(m.end_file + 65) << m.end_rank + 1 << " CHECKMATE\n";
                    } // print line if it ends in checkmate

                    vector<Move> fifth = b->generate_all_moves(Color::WHITE);

                    for (const auto & c : fifth) {

                        num_lines++;
                        
                        cout << num_lines << ": " 
                             << static_cast<char>(i.start_file + 65) << i.start_rank + 1 << " to "
                             << static_cast<char>(i.end_file + 65) << i.end_rank + 1 << ", "
                             << static_cast<char>(j.start_file + 65) << j.start_rank + 1 << " to "
                             << static_cast<char>(j.end_file + 65) << j.end_rank + 1 << ", "
                             << static_cast<char>(k.start_file + 65) << k.start_rank + 1 << " to "
                             << static_cast<char>(k.end_file + 65) << k.end_rank + 1 << ", "
                             << static_cast<char>(m.start_file + 65) << m.start_rank + 1 << " to "
                             << static_cast<char>(m.end_file + 65) << m.end_rank + 1 << ", "
                             << static_cast<char>(c.start_file + 65) << c.start_rank + 1 << " to "
                             << static_cast<char>(c.end_file + 65) << c.end_rank + 1 << "\n";

                    } // for c

                    b->undo_move(m, p4);
                } // for m

                b->undo_move(k, p3);
            } // for k

            b->undo_move(j, p2);
        } // for j

        b->undo_move(i, p1);
    } // for i


    cout << "\n\nnumber of lines generated after 5 perfs: " << num_lines << "\n\n";
    delete b;
} // test 5 perfs



void test_two_moves_each() {

    cout << "Testing the number of moves after two moves each (White and Black)\n\n";
    Board b;

    // Generate and store all White moves from the starting position
    vector<Move> white_moves = b.generate_all_moves(Color::WHITE);
    int initial_white_moves = (int)white_moves.size();

    if (white_moves.empty()) {
        cout << "No moves generated for White in the starting position.\n";
        return;
    }

    // Print the initial count of White moves and known theoretical perft references
    cout << "White moves from the starting position: " << initial_white_moves << endl;
    cout << "(For reference: perft(1) = 20 moves in standard chess from the initial position.)" << endl;
    cout << "(For reference: perft(2) = 400 positions, and perft(4) = 197,281 positions.)" << endl;
    cout << "Note: We are NOT doing a perft calculation here, just summing available moves.\n\n";

    int total_black_moves_after_white_first = 0;
    int total_white_moves_after_black_first = 0;
    int total_black_moves_after_white_second = 0;

    // Iterate over all possible first White moves
    for (const auto& white_move : white_moves) {
        Piece captured_white = b.get_piece_at(white_move.end_rank, white_move.end_file);
        b.make_move(white_move);

        // Generate Black moves after White's first move
        vector<Move> black_moves = b.generate_all_moves(Color::BLACK);
        total_black_moves_after_white_first += (int)black_moves.size(); 

        // Iterate over all possible first Black moves
        for (const auto& black_move : black_moves) {
            Piece captured_black = b.get_piece_at(black_move.end_rank, black_move.end_file);
            b.make_move(black_move);

            // Generate White's second moves
            vector<Move> second_white_moves = b.generate_all_moves(Color::WHITE);
            total_white_moves_after_black_first += (int)second_white_moves.size(); 

            // Iterate over all possible second White moves
            for (const auto& second_white_move : second_white_moves) {
                Piece captured_second_white = b.get_piece_at(second_white_move.end_rank, second_white_move.end_file);
                b.make_move(second_white_move);

                // Generate Black's second moves
                vector<Move> second_black_moves = b.generate_all_moves(Color::BLACK);
                total_black_moves_after_white_second += (int)second_black_moves.size(); 

                // Undo Black's second moves
                for (const auto& second_black_move : second_black_moves) {
                    Piece captured_second_black = b.get_piece_at(second_black_move.end_rank, second_black_move.end_file);
                    b.make_move(second_black_move);
                    b.undo_move(second_black_move, captured_second_black);
                }

                // Undo White's second move
                b.undo_move(second_white_move, captured_second_white);
            }

            // Undo Black's first move
            b.undo_move(black_move, captured_black);
        }

        // Undo White's first move
        b.undo_move(white_move, captured_white);
    }

    // Print the total counts
    cout << "\nFinal Tally of Moves:" << endl;
    cout << "Total Black moves after White's first move: " << total_black_moves_after_white_first << endl;
    cout << "Total White moves after Black's first move: " << total_white_moves_after_black_first << endl;
    cout << "Total Black moves after White's second move: " << total_black_moves_after_white_second << endl;
}


void test_generate_all_moves_depth3() {

    Board b;

    size_t white_first_moves_count = 0;
    size_t black_first_moves_count = 0;
    size_t white_second_moves_count = 0;

    // First move by White
    vector<Move> white_moves = b.generate_all_moves(Color::WHITE);
    white_first_moves_count = white_moves.size();

    // First move by Black
    for (const auto &white_move : white_moves) {
        Piece captured_white = b.get_piece_at(white_move.end_rank, white_move.end_file);
        b.make_move(white_move);

        vector<Move> black_moves = b.generate_all_moves(Color::BLACK);
        black_first_moves_count += black_moves.size();

        // Second move by White
        for (const auto & black_move : black_moves) {
            Piece captured_black = b.get_piece_at(black_move.end_rank, black_move.end_file);
            b.make_move(black_move);

            vector<Move> second_white_moves = b.generate_all_moves(Color::WHITE);
            white_second_moves_count += second_white_moves.size();

            // Print all moves after White's second move
            for (const auto &second_white_move : second_white_moves) {
                cout << static_cast<char>(65 + white_move.start_file) << white_move.start_rank + 1 << " to "
                        << static_cast<char>(65 + white_move.end_file) << white_move.end_rank + 1 << ", "
                        << static_cast<char>(65 + black_move.start_file) << black_move.start_rank + 1 << " to "
                        << static_cast<char>(65 + black_move.end_file) << black_move.end_rank + 1 << ", "
                        << static_cast<char>(65 + second_white_move.start_file) << second_white_move.start_rank + 1 << " to "
                        << static_cast<char>(65 + second_white_move.end_file) << second_white_move.end_rank + 1 << "\n";
            }

            // Undo White's second move
            b.undo_move(black_move, captured_black);
        }

        // Undo Black's first move
        b.undo_move(white_move, captured_white);
    }

    // Print the number of moves after each half-turn
    cout << "Number of moves after White's first move: " << white_first_moves_count << endl;
    cout << "Number of moves after Black's first move: " << black_first_moves_count << endl;
    cout << "Number of moves after White's second move: " << white_second_moves_count << endl;
}

void test_undo_move() {

    cout << "Testing undo move\n\n";

    Board b;
    BoardState before = b.get_current_board_state();
    Move e2e4 = Move{5,2,5,4};
    Piece captured = b.get_piece_at(e2e4.end_file, e2e4.end_rank);

    b.generate_all_moves(Color::WHITE);
    b.make_move(e2e4);
    b.undo_move(e2e4, captured);

    BoardState state_after_undo_move = b.get_current_board_state();

    assert(before == state_after_undo_move);
    before = state_after_undo_move; // for compiler

}

void test_undo_capture() {

    cout << "Testing undo after capture\n\n";

    Board b;
    Move e7e5 = Move{5, 7, 5, 5};
    b.generate_board(); // Setup initial position

    // Move a black pawn forward to create a capture opportunity
    b.make_move(e7e5);

    // White pawn captures black pawn
    Move f2e5 = Move{6, 2, 5, 5};
    Piece captured = b.get_piece_at(f2e5.end_rank, f2e5.end_file);
    BoardState before_capture = b.get_current_board_state();

    b.make_move(f2e5);

    // Ensure the move was made correctly
    assert(b.get_piece_at(5, 5).getType() == PieceType::PAWN);
    assert(b.get_piece_at(5, 5).getColor() == Color::WHITE);

    // Undo the move
    b.undo_move(f2e5, captured);

    // Ensure the state is restored
    BoardState after_undo = b.get_current_board_state();
    assert(before_capture == after_undo);
    before_capture = after_undo; // for compiler

    cout << "Undo after capture test passed!\n";
}


uint64_t moveGenerationTest(int depth, Color c, Board * b) {

    if (depth == 0)
        return 1;

    vector<Move> moves = b->generate_all_moves(c);
    uint64_t num_positions = 0;

    for (const auto & m : moves) {
        // Correctly retrieve the piece being captured
        Piece p = b->get_piece_at(m.end_rank, m.end_file);
        b->make_move(m);

        // Recursive call with reduced depth and toggled color
        num_positions += moveGenerationTest(depth - 1, (c == Color::WHITE ? Color::BLACK : Color::WHITE), b);

        // Undo the move
        b->undo_move(m, p);
    }

    return num_positions;
}

uint64_t FromLayout(const char* layout_str) {
  uint64_t bitboard = 0ULL;
  int sq = 0;
  for (int i = 0; layout_str[i] != '\0'; ++i) {
    if (layout_str[i] != '.' && layout_str[i] != '1') {
      continue;
    }
    const int index = (7 - (sq / 8)) * 8 + (sq % 8);
    sq++;
    if (sq > 64) {
      throw std::invalid_argument("Too many squares");
    }
    if (layout_str[i] == '.') {
      continue;
    }
    bitboard |= (1ULL << index);
  }
  if (sq < 64) {
    throw std::invalid_argument("Too few squares");
  }
  return bitboard;
}


void test_bitboards() {
    magic_bits::Attacks attacks;
    [[maybe_unused]] const char* layout_str = R"""(
        1.11..1.
        .1...1.1
        1....11.
        ........
        ....1...
        ...1.1.1
        111..11.
        1....11.
    )""";
    assert(FromLayout(R"""(
        ........
        .1......
        .1...1..
        .1..1...
        .1.1....
        111.....
        1.1.....
        111.....
    )""") == attacks.Queen(FromLayout(layout_str), 9));
    assert(FromLayout(R"""(
        ...1...1
        ....1.1.
        ........
        ....1.1.
        ...1...1
        ..1.....
        .1......
        ........
    )""") == attacks.Bishop(FromLayout(layout_str), 45));
    assert(FromLayout(R"""(
        ..1.111.
        ...1....
        ...1....
        ...1....
        ...1....
        ...1....
        ........
        ........
    )""") == attacks.Rook(FromLayout(layout_str), 59));
    std::cout << "TESTS PASSED" << std::endl;

}





#endif
