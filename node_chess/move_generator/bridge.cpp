#include "bridge.h"
#include "board.h"  // your existing header with Board, Move, Color, etc.

// Minimal helpers to convert between your Move and the C POD MoveC
static inline Move to_cpp(const MoveC& m) {
    Move cpp{};
    cpp.start_file = m.start_file;
    cpp.start_rank = m.start_rank;
    cpp.end_file   = m.end_file;
    cpp.end_rank   = m.end_rank;
    return cpp;
}
static inline MoveC to_c(const Move& m) {
    MoveC c{};
    c.start_file = m.start_file;
    c.start_rank = m.start_rank;
    c.end_file   = m.end_file;
    c.end_rank   = m.end_rank;
    return c;
}

extern "C" {

Board* board_new() {
    // Your Board constructor already calls init() -> generate_board(), magic, precomputed attacks.
    // If you want the default startpos, this is enough; otherwise call board_generate_startpos explicitly.
    return new Board();
}

void board_delete(Board* b) {
    delete b;
}

void board_generate_startpos(Board* b) {
    if (b) b->generate_board();
}

void board_set_turn_index(Board* b, int ti) {
    if (b) b->set_turn_index(ti);
}

void board_compute_attack_masks(Board* b) {
    if (b) b->compute_attack_masks();
}

size_t board_generate_all_moves(Board* b, int color_int, MoveC** out_moves) {
    if (!b || !out_moves) return 0;

    Color color = (color_int == 0) ? Color::WHITE : Color::BLACK;
    // Your code routes generate_all_moves -> generate_legal_moves(color)
    // and uses Move{start_file,start_rank,end_file,end_rank}. :contentReference[oaicite:2]{index=2}
    std::vector<Move> v = b->generate_all_moves(color);

    size_t n = v.size();
    if (n == 0) {
        *out_moves = nullptr;
        return 0;
    }

    MoveC* arr = new MoveC[n];
    for (size_t i = 0; i < n; ++i) {
        arr[i] = to_c(v[i]);
    }
    *out_moves = arr;
    return n;
}

void board_free_moves(MoveC* arr) {
    delete[] arr;
}

Piece* board_make_move(Board* b, const MoveC* m) {
    if (!b || !m) return nullptr;

    // Capture the piece that currently sits on the destination square *before* making the move.
    // We keep it on the heap as an opaque handle so Python doesn't need to know the Piece layout.
    Move cpp = to_cpp(*m);
    const Piece& captured = b->get_piece_at(cpp.end_rank, cpp.end_file); // uses your overloads. :contentReference[oaicite:3]{index=3}

    Piece* captured_handle = new Piece(captured); // may be NONE/NONE if empty
    b->make_move(cpp);                            // your make_move updates stacks/bitboards. :contentReference[oaicite:4]{index=4}
    return captured_handle;
}

void board_undo_move(Board* b, const MoveC* m, Piece* handle) {
    if (!b || !m) return;
    Move cpp = to_cpp(*m);

    // If handle is null, construct an empty piece as "no capture".
    if (!handle) {
        Piece empty(PieceType::NONE, Color::NONE);
        b->undo_move(cpp, empty);
        return;
    }

    b->undo_move(cpp, *handle);  // your undo_move expects the captured Piece by value. :contentReference[oaicite:5]{index=5}
    delete handle;               // free the opaque captured-piece handle
}

} // extern "C"

