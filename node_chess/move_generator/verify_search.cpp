#include "board.h"
#include "search.h"
#include <iostream>

static int count_pieces(Board& b) {
    int n = 0;
    for (int r = 0; r < 8; r++) {
        for (int f = 0; f < 8; f++) {
            if (b.get_piece_at(r, f).getType() != PieceType::NONE) {
                n++;
            }
        }
    }
    return n;
}

int main() {
    Board b;
    b.clear();
    b.generate_board();
    search_rebuild_bitboards_from_2d(b);

    auto legal0 = b.generate_all_moves(Color::WHITE);
    if (legal0.size() != 20) {
        std::cerr << "FAIL: expected 20 root moves, got " << legal0.size() << "\n";
        return 1;
    }

    Searcher s;
    int sc = 0, d = 0;
    uint64_t nodes = 0;
    Move best = s.search(b, Color::WHITE, 4, 8000, sc, d, nodes);

    if (best.start_rank < 0 || best.start_file < 0) {
        std::cerr << "FAIL: search returned no move\n";
        return 1;
    }

    search_rebuild_bitboards_from_2d(b);
    auto legal1 = b.generate_all_moves(Color::WHITE);
    if (legal1.size() != 20) {
        std::cerr << "FAIL: after search expected 20 moves, got " << legal1.size() << "\n";
        return 1;
    }

    if (count_pieces(b) != 32) {
        std::cerr << "FAIL: piece count after search " << count_pieces(b) << "\n";
        return 1;
    }

    std::cout << "ok search d=" << d << " score=" << sc << " nodes=" << nodes << " best="
              << char('a' + best.start_file) << (1 + best.start_rank)
              << char('a' + best.end_file) << (1 + best.end_rank) << "\n";
    return 0;
}
