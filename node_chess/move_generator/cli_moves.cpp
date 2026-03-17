#include "board.h"  
#include <iostream>
#include <string>
#include <vector>
#include <cctype>

using namespace std;


extern "C" {
    Board * Board_new() { return new Board(); }
    void Board_move(Board * b, Move * m) { b->make_move(*m); }
    void Board_undo(Board * b, Move * m, Piece * captured) { b->undo_move(*m, *captured); }
}


// file/rank are 0..7 in your Move; convert to "e2" style
static string sq(int file, int rank) {
    return string() + char('a' + file) + char('1' + rank);
}

// parse "e2e4" → Move{4,1,4,3}  (files a=0..h=7, ranks 1..8 → 0..7)
static bool parse_uci_move(const std::string & s, Move & out) {
    if (s.size() < 4) return false;
    int sf = s[0] - 'a';
    int sr = s[1] - '1';
    int ef = s[2] - 'a';
    int er = s[3] - '1';
    if (sf < 0 || sf > 7 || ef < 0 || ef > 7 || sr < 0 || sr > 7 || er < 0 || er > 7) return false;
    out.start_file = sf; out.start_rank = sr;
    out.end_file   = ef; out.end_rank   = er;
    return true;
}

int main(int argc, char** argv) {
    Board * b = new Board();
    b->generate_board();       

    // Represent board state
    for (int i = 1; i < argc; ++i) {
        std::string uci = argv[i];
        Move m{};
        if (!parse_uci_move(uci, m)) continue;
        b->make_move(m);
    }

    // Color who's turn to move
    Color side = b->get_color();

    // Generate moves
    std::vector<Move> moves = b->generate_legal_moves(side);

    // Print one move per line as UCI "e2e4"
    for (const auto & mv : moves) {
        cout << sq(mv.start_file, mv.start_rank)
             << sq(mv.end_file, mv.end_rank) << "\n";
    }
    return 0;
}
