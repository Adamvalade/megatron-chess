// standard ChessProgrammingWiki perft suite:
//   https://www.chessprogramming.org/Perft_Results
//
// Usage:
//   ./test_perft                     run the suite (with undo checks)
//   ./test_perft --max-depth N       only test depths <= N
//   ./test_perft --no-check          skip make/undo round-trip checks
//   ./test_perft --divide D INDEX    per-root-move counts for position INDEX
//                                    (0-based, in the order listed below)
//
// Exit status: 0 if every perft comparison (and undo check, if enabled)
// passes, 1 otherwise. Known-failing positions are intentional: they are
// preserved as evidence of existing bugs and must NOT be edited away here.
//
// Focused unit tests for specific make/undo state-restoration bugs live in
// run_unit_tests() below; run them alone with --unit. They are regression
// tests: each one failed before the fix it guards.
//
// Build: make test_perft   (uses the Makefile's existing test* machinery)

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "board.h"
#include "eval.h"
#include "pstTables.h"
#include "search.h"
#include "zobrist.h"

using namespace std;

namespace {

struct Position {
    const char * name;
    const char * fen;
    // (depth, expected leaf-node count) pairs, shallow to deep.
    vector<pair<int, uint64_t>> depths;
};

// Standard ChessProgrammingWiki perft suite.
const vector<Position> POSITIONS = {
    {"startpos",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     {{1, 20}, {2, 400}, {3, 8902}, {4, 197281}, {5, 4865609}}},
    {"kiwipete",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {{1, 48}, {2, 2039}, {3, 97862}, {4, 4085603}}},
    {"cpw3 en-passant pins",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     {{1, 14}, {2, 191}, {3, 2812}, {4, 43238}, {5, 674624}}},
    {"cpw4 promotions/castling",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     {{1, 6}, {2, 264}, {3, 9467}, {4, 422333}}},
    {"cpw5",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     {{1, 44}, {2, 1486}, {3, 62379}, {4, 2103487}}},
    {"cpw6",
     "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     {{1, 46}, {2, 2079}, {3, 89890}, {4, 3894594}}},
};

// ---------------------------------------------------------------------------
// Make/undo round-trip checking
// ---------------------------------------------------------------------------

struct UndoCheck {
    bool enabled = true;
    uint64_t violations = 0;
    // Violations where the only difference is the en-passant square:
    // stale state, but the pieces themselves were restored correctly.
    uint64_t ep_only_violations = 0;
    // Violations involving piece placement, a piece bitboard, castling
    // rights, or turn bookkeeping: the board itself was corrupted.
    uint64_t board_violations = 0;
    uint64_t reported = 0;       // en-passant-only examples shown
    uint64_t reported_board = 0; // board-corrupting examples shown
};

// Everything undo_move() is supposed to restore. BoardState covers the 12
// piece bitboards, castling rights and the en-passant square; the FEN
// placement string additionally covers the redundant 8x8 board array, and
// turn_index covers side-to-move bookkeeping.
struct Snapshot {
    BoardState state;
    string placement;
    size_t turn_index;
};

Snapshot take_snapshot(Board &board) {
    const string fen = board.to_fen();
    return Snapshot{board.get_current_board_state(),
                    fen.substr(0, fen.find(' ')),
                    board.get_turn_index()};
}

string move_to_string(const Move &m) {
    string s;
    s += static_cast<char>('a' + m.start_file);
    s += static_cast<char>('1' + m.start_rank);
    s += static_cast<char>('a' + m.end_file);
    s += static_cast<char>('1' + m.end_rank);
    // UCI-style promotion suffix, so divide output and move comparisons
    // distinguish the four promotion choices (a7a8q vs a7a8n, ...).
    switch (m.promotion) {
        case PieceType::QUEEN:  s += 'q'; break;
        case PieceType::ROOK:   s += 'r'; break;
        case PieceType::BISHOP: s += 'b'; break;
        case PieceType::KNIGHT: s += 'n'; break;
        default: break;
    }
    return s;
}

string differing_fields(const BoardState &a, const BoardState &b) {
    string fields;
    auto note = [&fields](bool same, const char *name) {
        if (!same) {
            if (!fields.empty()) {
                fields += ", ";
            }
            fields += name;
        }
    };
    note(a.white_pawns == b.white_pawns, "white_pawns");
    note(a.white_knights == b.white_knights, "white_knights");
    note(a.white_bishops == b.white_bishops, "white_bishops");
    note(a.white_rooks == b.white_rooks, "white_rooks");
    note(a.white_queens == b.white_queens, "white_queens");
    note(a.white_kings == b.white_kings, "white_kings");
    note(a.black_pawns == b.black_pawns, "black_pawns");
    note(a.black_knights == b.black_knights, "black_knights");
    note(a.black_bishops == b.black_bishops, "black_bishops");
    note(a.black_rooks == b.black_rooks, "black_rooks");
    note(a.black_queens == b.black_queens, "black_queens");
    note(a.black_kings == b.black_kings, "black_kings");
    note(a.castling_rights == b.castling_rights, "castling_rights");
    note(a.en_passant_square == b.en_passant_square, "en_passant_square");
    return fields;
}

void check_round_trip(Board &board, const Snapshot &before, const Move &m,
                      UndoCheck &check) {
    const Snapshot after = take_snapshot(board);
    if (after.state == before.state && after.placement == before.placement &&
        after.turn_index == before.turn_index) {
        return;
    }
    ++check.violations;
    const string fields = differing_fields(before.state, after.state);
    const bool ep_only = after.placement == before.placement &&
                         after.turn_index == before.turn_index &&
                         (fields.empty() || fields == "en_passant_square");
    bool show = false;
    if (ep_only) {
        ++check.ep_only_violations;
        show = check.reported < 3;
        check.reported += show ? 1 : 0;
    } else {
        ++check.board_violations;
        show = check.reported_board < 5;
        check.reported_board += show ? 1 : 0;
    }
    if (show) {
        cout << "    UNDO VIOLATION after undoing " << move_to_string(m)
             << "\n      placement before: " << before.placement
             << "\n      placement after:  " << after.placement;
        if (!(after.state == before.state)) {
            cout << "\n      BoardState differs in: " << fields;
        }
        if (after.turn_index != before.turn_index) {
            cout << "\n      turn_index differs: " << before.turn_index
                 << " -> " << after.turn_index;
        }
        cout << "\n";
    }
}

// ---------------------------------------------------------------------------
// Perft
// ---------------------------------------------------------------------------

uint64_t perft(Board &board, int depth, Color side, UndoCheck &check) {
    if (depth == 0) {
        return 1;
    }

    const vector<Move> moves = board.generate_all_moves(side);
    const Color next = (side == Color::WHITE) ? Color::BLACK : Color::WHITE;

    uint64_t nodes = 0;
    for (const Move &m : moves) {
        // get_piece_at returns a reference into the board: copy it before
        // make_move mutates the board.
        const Piece captured = board.get_piece_at(m.end_rank, m.end_file);
        Snapshot before{};
        if (check.enabled) {
            before = take_snapshot(board);
        }
        board.make_move(m);
        nodes += perft(board, depth - 1, next, check);
        board.undo_move(m, captured);
        if (check.enabled) {
            check_round_trip(board, before, m, check);
        }
    }
    return nodes;
}

// ---------------------------------------------------------------------------
// Focused unit tests (regression tests for specific fixed bugs)
// ---------------------------------------------------------------------------

Move parse_move_str(const char *s) {
    Move m{s[0] - 'a', s[1] - '1', s[2] - 'a', s[3] - '1'};
    if (s[4] != '\0') { // optional promotion suffix, e.g. "a7a8q"
        switch (s[4]) {
            case 'q': m.promotion = PieceType::QUEEN;  break;
            case 'r': m.promotion = PieceType::ROOK;   break;
            case 'b': m.promotion = PieceType::BISHOP; break;
            case 'n': m.promotion = PieceType::KNIGHT; break;
            default: break;
        }
    }
    return m;
}

bool snapshot_equal(const Snapshot &a, const Snapshot &b) {
    return a.state == b.state && a.placement == b.placement &&
           a.turn_index == b.turn_index;
}

// Apply a move exactly as perft does (copy the captured piece first).
Piece apply(Board &board, const char *move_str) {
    const Move m = parse_move_str(move_str);
    const Piece captured = board.get_piece_at(m.end_rank, m.end_file);
    board.make_move(m);
    return captured;
}

void retract(Board &board, const char *move_str, const Piece &captured) {
    board.undo_move(parse_move_str(move_str), captured);
}

int unit_failures = 0;

void unit_check(bool ok, const char *name) {
    cout << "  " << (ok ? "PASS" : "FAIL") << "  " << name << "\n";
    if (!ok) {
        ++unit_failures;
    }
}

// Undoing a double pawn push must restore the previous en-passant state
// (none), not leave the just-created EP square behind.
void test_ep_undo_double_push() {
    Board board;
    const Snapshot initial = take_snapshot(board);
    const Piece cap = apply(board, "e2e4");
    unit_check(board.getEnPassantSquare() == 20,
               "ep set to e3 after e2e4"); // e3 = rank 2 * 8 + file 4
    retract(board, "e2e4", cap);
    unit_check(snapshot_equal(take_snapshot(board), initial),
               "ep/state restored after undoing e2e4");
}

// Two double pushes in a row: undoing the second must restore the first
// one's EP square, and undoing the first must restore "none".
void test_ep_consecutive_double_pushes() {
    Board board;
    const Snapshot initial = take_snapshot(board);
    const Piece cap1 = apply(board, "e2e4");
    const Snapshot after_e4 = take_snapshot(board);
    const Piece cap2 = apply(board, "e7e5");
    unit_check(board.getEnPassantSquare() == 44,
               "ep set to e6 after e7e5"); // e6 = rank 5 * 8 + file 4
    retract(board, "e7e5", cap2);
    unit_check(board.getEnPassantSquare() == 20 &&
                   snapshot_equal(take_snapshot(board), after_e4),
               "ep restored to e3 after undoing e7e5");
    retract(board, "e2e4", cap1);
    unit_check(snapshot_equal(take_snapshot(board), initial),
               "state restored after undoing both double pushes");
}

// A quiet (non-pawn) move clears the EP square; undoing it must bring the
// previous EP square back.
void test_ep_quiet_move_clears_and_restores() {
    Board board;
    apply(board, "e2e4");
    const Snapshot after_e4 = take_snapshot(board);
    const Piece cap = apply(board, "g1f3");
    unit_check(board.getEnPassantSquare() == 0xFF,
               "ep cleared by Ng1f3");
    retract(board, "g1f3", cap);
    unit_check(board.getEnPassantSquare() == 20 &&
                   snapshot_equal(take_snapshot(board), after_e4),
               "ep restored to e3 after undoing Ng1f3");
}

// En-passant capture itself: make must remove the captured pawn, undo
// must restore the whole position.
void test_ep_capture_make_undo() {
    Board board;
    apply(board, "e2e4");
    apply(board, "a7a6");
    apply(board, "e4e5");
    apply(board, "d7d5");
    const Snapshot before = take_snapshot(board);
    const Piece cap = apply(board, "e5d6");
    unit_check(board.get_piece_at(4, 3).getType() == PieceType::NONE,
               "ep capture removes the pawn on d5");
    retract(board, "e5d6", cap);
    unit_check(snapshot_equal(take_snapshot(board), before),
               "state restored after undoing ep capture e5d6");
}

// Ordinary capture undo (control case for the EP tests).
void test_capture_make_undo() {
    Board board;
    apply(board, "e2e4");
    apply(board, "d7d5");
    const Snapshot before = take_snapshot(board);
    const Piece cap = apply(board, "e4d5");
    retract(board, "e4d5", cap);
    unit_check(snapshot_equal(take_snapshot(board), before),
               "state restored after undoing capture e4xd5");
}

// Castling is not a pawn move: it must clear any EP square set by the
// opponent's preceding double push, and undoing the castle must restore
// that EP square.
void test_ep_castling_clears_and_restores() {
    Board board;
    board.set_from_fen("r3k2r/p7/8/8/8/8/8/R3K2R b KQkq - 0 1");
    apply(board, "a7a5"); // double push: EP square a6 = 40
    const Snapshot before_castle = take_snapshot(board);
    unit_check(board.getEnPassantSquare() == 40, "ep set to a6 after a7a5");
    const Piece cap = apply(board, "e1g1"); // White castles kingside
    unit_check(board.getEnPassantSquare() == 0xFF,
               "ep cleared by castling e1g1");
    retract(board, "e1g1", cap);
    unit_check(board.getEnPassantSquare() == 40 &&
                   snapshot_equal(take_snapshot(board), before_castle),
               "ep restored to a6 after undoing castling");
}

// An EP target loaded from FEN must be usable by move generation and
// must survive a make/undo round trip of an unrelated move.
void test_ep_from_fen() {
    Board board;
    board.set_from_fen(
        "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
    bool ep_move_generated = false;
    for (const Move &m : board.generate_all_moves(Color::WHITE)) {
        if (move_to_string(m) == "e5f6") {
            ep_move_generated = true;
        }
    }
    unit_check(ep_move_generated, "ep capture e5f6 generated from FEN");

    const Snapshot before = take_snapshot(board);
    const Piece cap = apply(board, "g1f3");
    retract(board, "g1f3", cap);
    unit_check(snapshot_equal(take_snapshot(board), before),
               "fen ep state restored after make/undo of Ng1f3");
}

// Castling rights must be revoked when a rook is captured on its home
// square. Regression test: after ...Nf2xh1, White must not be offered
// e1g1 castling, and generating White's moves must not change the board
// (previously the castling make/undo inside move filtering deleted the
// knight on h1 and resurrected the captured rook).
void test_castling_rights_revoked_on_rook_capture() {
    Board board;
    board.set_from_fen("4k3/8/8/8/8/8/5n2/R3K2R w KQ - 0 1");
    apply(board, "f2h1"); // black knight captures the h1 rook
    unit_check((board.getCastlingRights() & WHITE_KINGSIDE) == 0,
               "white kingside right revoked after rook captured on h1");

    const Snapshot before = take_snapshot(board);
    bool castle_offered = false;
    for (const Move &m : board.generate_all_moves(Color::WHITE)) {
        if (move_to_string(m) == "e1g1") {
            castle_offered = true;
        }
    }
    unit_check(!castle_offered,
               "castling e1g1 not generated without a rook on h1");
    unit_check(snapshot_equal(take_snapshot(board), before),
               "move generation does not modify the board (Nxh1 position)");
}

// Moving a rook (or the king) must revoke the corresponding castling
// right, and undoing the move must restore it.
void test_castling_rights_revoked_on_rook_and_king_moves() {
    Board board;
    board.set_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const uint16_t all_rights = board.getCastlingRights();

    const Piece cap1 = apply(board, "h1h2");
    unit_check((board.getCastlingRights() & WHITE_KINGSIDE) == 0 &&
                   (board.getCastlingRights() & WHITE_QUEENSIDE) != 0,
               "white kingside right revoked after Rh1h2");
    retract(board, "h1h2", cap1);
    unit_check(board.getCastlingRights() == all_rights,
               "castling rights restored after undoing Rh1h2");

    const Piece cap2 = apply(board, "e1e2");
    unit_check((board.getCastlingRights() &
                (WHITE_KINGSIDE | WHITE_QUEENSIDE)) == 0,
               "both white rights revoked after Ke1e2");
    retract(board, "e1e2", cap2);
    unit_check(board.getCastlingRights() == all_rights,
               "castling rights restored after undoing Ke1e2");
}

// ---------------------------------------------------------------------------
// Promotion tests. Each legal promotion (quiet or capture, either color)
// must be generated as four distinct moves, make_move must create the
// chosen piece, and undo_move must restore the pawn and any captured
// piece. These tests use only generated Move objects, so they compile
// against a Move type with or without a promotion field -- and fail at
// runtime until all four choices are actually generated and executed.
// ---------------------------------------------------------------------------

vector<Move> generated_moves(Board &board, const char *from_to) {
    const Move target = parse_move_str(from_to);
    vector<Move> result;
    const Color side = board.get_color();
    for (const Move &m : board.generate_all_moves(side)) {
        if (m.start_file == target.start_file &&
            m.start_rank == target.start_rank &&
            m.end_file == target.end_file && m.end_rank == target.end_rank) {
            result.push_back(m);
        }
    }
    return result;
}

// Make each candidate move in turn: the four promotion choices must
// produce four distinct displayed moves and all four piece types, and
// every make must undo back to the exact starting snapshot.
void check_promotion_choices(Board &board, const char *from_to,
                             const char *label) {
    const Snapshot before = take_snapshot(board);
    const vector<Move> candidates = generated_moves(board, from_to);

    set<string> displays;
    set<PieceType> types_made;
    bool undo_ok = true;
    for (const Move &m : candidates) {
        displays.insert(move_to_string(m));
        const Piece captured = board.get_piece_at(m.end_rank, m.end_file);
        board.make_move(m);
        types_made.insert(board.get_piece_at(m.end_rank, m.end_file).getType());
        board.undo_move(m, captured);
        if (!snapshot_equal(take_snapshot(board), before)) {
            undo_ok = false;
        }
    }

    const string prefix = string(label) + " " + from_to;
    unit_check(candidates.size() == 4 && displays.size() == 4,
               (prefix + ": 4 distinct promotion moves generated").c_str());
    unit_check(types_made == set<PieceType>{PieceType::QUEEN, PieceType::ROOK,
                                            PieceType::BISHOP,
                                            PieceType::KNIGHT},
               (prefix + ": Q/R/B/N all created by make_move").c_str());
    unit_check(undo_ok,
               (prefix + ": every promotion undoes to the exact position")
                   .c_str());
}

void test_promotions() {
    { // White quiet promotion a7a8
        Board board;
        board.set_from_fen("8/P7/8/8/8/8/k6K/8 w - - 0 1");
        check_promotion_choices(board, "a7a8", "white quiet promotion");
    }
    { // White promotion capture a7xb8 (and the quiet push alongside)
        Board board;
        board.set_from_fen("1r6/P7/8/8/8/8/k6K/8 w - - 0 1");
        check_promotion_choices(board, "a7b8", "white promotion capture");
        check_promotion_choices(board, "a7a8", "white quiet promotion (2)");
    }
    { // Black quiet promotion h2h1
        Board board;
        board.set_from_fen("7k/8/8/8/8/8/K6p/8 b - - 0 1");
        check_promotion_choices(board, "h2h1", "black quiet promotion");
    }
    { // Black promotion capture h2xg1
        Board board;
        board.set_from_fen("7k/8/8/8/8/8/K6p/6R1 b - - 0 1");
        check_promotion_choices(board, "h2g1", "black promotion capture");
    }
    { // Backward compatibility: a hand-built pawn move to the last rank
      // with no promotion choice still promotes to a queen.
        Board board;
        board.set_from_fen("8/P7/8/8/8/8/k6K/8 w - - 0 1");
        const Snapshot before = take_snapshot(board);
        const Piece cap = apply(board, "a7a8");
        unit_check(board.get_piece_at(7, 0).getType() == PieceType::QUEEN,
                   "promotion with no choice defaults to queen");
        retract(board, "a7a8", cap);
        unit_check(snapshot_equal(take_snapshot(board), before),
                   "default-queen promotion undoes cleanly");
    }
}

// A default-constructed Board must be the standard starting position:
// White to move, all castling rights, no en-passant square.
void test_default_board_initialization() {
    Board board;
    unit_check(board.to_fen() ==
                   "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
               "default board FEN is the starting position");
    unit_check(board.getCastlingRights() ==
                   (WHITE_KINGSIDE | WHITE_QUEENSIDE | BLACK_KINGSIDE |
                    BLACK_QUEENSIDE),
               "default board has all castling rights");
    unit_check(board.getEnPassantSquare() == 0xFF,
               "default board has no en-passant square");
    unit_check(board.get_color() == Color::WHITE && board.get_turn_index() == 0,
               "default board: White to move at turn 0");
    unit_check(board.generate_all_moves(Color::WHITE).size() == 20,
               "default board: 20 legal moves for White");
}

// Stalemate = side is not in check and has no legal moves. Checkmate =
// in check with no legal moves. Neither query may mutate the board.
void check_terminal(Board &board, Color side, bool expect_mate,
                    bool expect_stalemate, const char *label) {
    const Snapshot before = take_snapshot(board);
    const bool mate = board.is_checkmate(side);
    const bool stale = board.is_stalemate(side);
    unit_check(mate == expect_mate && stale == expect_stalemate, label);
    unit_check(snapshot_equal(take_snapshot(board), before),
               (string(label) + " (board unchanged by queries)").c_str());
}

void test_checkmate_and_stalemate() {
    { // Starting position: game in progress for both sides.
        Board board;
        check_terminal(board, Color::WHITE, false, false,
                       "startpos: neither mate nor stalemate (White)");
        check_terminal(board, Color::BLACK, false, false,
                       "startpos: neither mate nor stalemate (Black)");
    }
    { // Fool's mate: White is checkmated by Qh4.
        Board board;
        board.set_from_fen(
            "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
        check_terminal(board, Color::WHITE, true, false,
                       "fool's mate: White is checkmated, not stalemated");
    }
    { // Classic stalemate: Black king h8, White Qf7 + Kg6. Not in check,
      // every flight square covered.
        Board board;
        board.set_from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        check_terminal(board, Color::BLACK, false, true,
                       "stalemate: Black to move is stalemated, not mated");
        check_terminal(board, Color::WHITE, false, false,
                       "stalemate position: White still has legal moves");
    }
    { // White stalemated: Ka1, black Qb3 + Kc1 cover a2/b1/b2, a1 itself
      // is not under attack.
        Board board;
        board.set_from_fen("8/8/8/8/8/1q6/8/K1k5 w - - 0 1");
        check_terminal(board, Color::WHITE, false, true,
                       "stalemate: White to move is stalemated, not mated");
    }
    { // In check but with escapes: neither mate nor stalemate.
        Board board;
        board.set_from_fen("4k3/8/8/8/8/8/4R3/4K3 b - - 0 1");
        check_terminal(board, Color::BLACK, false, false,
                       "in check with flight squares: neither terminal");
    }
}

// ---------------------------------------------------------------------------
// Static evaluation tests (R6). Scores are centipawns from the side to
// move's perspective.
// ---------------------------------------------------------------------------

void test_evaluation() {
    { // Starting position is exactly equal.
        Board board;
        unit_check(evaluate_board(board) == 0,
                   "eval: starting position is 0");
    }
    { // Material advantage + perspective: same placement, score must
      // flip sign exactly when the side to move flips.
        Board white_to_move;
        white_to_move.set_from_fen("7k/8/8/8/8/8/8/Q6K w - - 0 1");
        Board black_to_move;
        black_to_move.set_from_fen("7k/8/8/8/8/8/8/Q6K b - - 0 1");
        const int as_white = evaluate_board(white_to_move);
        const int as_black = evaluate_board(black_to_move);
        unit_check(as_white == 880,
                   "eval: queen up scores +880 for White to move");
        unit_check(as_black == -as_white,
                   "eval: score negates when side to move changes");
    }
    {
        Board central;
        central.set_from_fen("7k/8/8/8/3N4/8/8/K7 w - - 0 1");
        Board corner;
        corner.set_from_fen("N6k/8/8/8/8/8/8/K7 w - - 0 1");
        unit_check(evaluate_board(central) - evaluate_board(corner) == 29,
                   "eval: Nd4 outscores Na8 by the scaled PST difference (29)");
    }
    { // Vertical-mirror symmetry: White Nd4/Ka1 vs Kh8 mirrors to Black
      // Nd5/Ka8 vs Kh1; with Black to move the scores must be equal.
        Board white;
        white.set_from_fen("7k/8/8/8/3N4/8/8/K7 w - - 0 1");
        Board mirrored;
        mirrored.set_from_fen("k7/8/8/3n4/8/8/8/7K b - - 0 1");
        unit_check(evaluate_board(white) == evaluate_board(mirrored),
                   "eval: vertically mirrored positions score equally");
    }
    { // Black PST orientation: a black knight centralized on d5 must
      // score as well for Black (to move) as the white one did above.
        Board black_central;
        black_central.set_from_fen("k7/8/8/3n4/8/8/8/7K b - - 0 1");
        Board black_corner;
        black_corner.set_from_fen("k7/8/8/8/8/8/8/n6K b - - 0 1");
        unit_check(evaluate_board(black_central) -
                           evaluate_board(black_corner) ==
                       29,
                   "eval: black Nd5 outscores black Na1 by 29");
    }
    { // Evaluation must not mutate the board.
        Board board;
        board.set_from_fen(
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w "
            "KQkq - 0 1");
        const Snapshot before = take_snapshot(board);
        (void)evaluate_board(board);
        unit_check(snapshot_equal(take_snapshot(board), before),
                   "eval: does not mutate the board");
    }
}

bool move_is_legal(Board &board, const Move &move) {
    for (const Move &legal : board.generate_all_moves(board.get_color())) {
        if (legal == move) {
            return true;
        }
    }
    return false;
}

bool delivers_checkmate(Board &board, const Move &move) {
    const Piece captured = board.get_piece_at(move.end_rank, move.end_file);
    board.make_move(move);
    const Color opponent = board.get_color();
    const bool mate = board.is_checkmate(opponent);
    board.undo_move(move, captured);
    return mate;
}

int reference_negamax(Board &board, int depth, int ply, uint64_t &nodes) {
    if (depth == 0) {
        uint64_t qnodes = 0;
        return search_quiescence(board, nodes, qnodes, ply);
    }
    ++nodes;
    const Color side = board.get_color();
    const vector<Move> moves = board.generate_all_moves(side);
    if (moves.empty()) {
        return board.is_check(side) ? -MATE_SCORE + ply : 0;
    }
    int best_score = -SEARCH_INF;
    for (const Move &move : moves) {
        const Piece captured = board.get_piece_at(move.end_rank, move.end_file);
        board.make_move(move);
        const int score = -reference_negamax(board, depth - 1, ply + 1, nodes);
        board.undo_move(move, captured);
        if (score > best_score) {
            best_score = score;
        }
    }
    return best_score;
}

int reference_root_score(Board &board, int depth) {
    const vector<Move> moves = board.generate_all_moves(board.get_color());
    if (moves.empty()) {
        return board.is_check(board.get_color()) ? -MATE_SCORE : 0;
    }
    int best_score = -SEARCH_INF;
    for (const Move &move : moves) {
        const Piece captured = board.get_piece_at(move.end_rank, move.end_file);
        board.make_move(move);
        uint64_t nodes = 0;
        const int score = -reference_negamax(board, depth - 1, 1, nodes);
        board.undo_move(move, captured);
        if (score > best_score) {
            best_score = score;
        }
    }
    return best_score;
}

void test_alpha_beta_matches_reference() {
    const char *fens[] = {
        "4k3/8/8/3n4/2B5/8/8/4K3 w - - 0 1",
        "4k3/8/8/3q4/8/2N5/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/3r4/8/3RK3 w - - 0 1",
        "r3k3/8/8/3N4/8/8/8/4K3 w - - 0 1",
        "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1",
    };
    for (const char *fen : fens) {
        for (int depth = 1; depth <= 4; ++depth) {
            Board ab_board;
            ab_board.set_from_fen(fen);
            const SearchResult ab_result = find_best_move(ab_board, depth);

            Board ref_board;
            ref_board.set_from_fen(fen);
            const int ref_score = reference_root_score(ref_board, depth);

            unit_check(ab_result.score == ref_score,
                       ("ab vs ref scores match at depth " + to_string(depth))
                           .c_str());
        }
    }
}

int material_white_perspective(const Board &board) {
    constexpr int values[6] = {100, 320, 330, 500, 900, 0};
    int score = 0;
    for (Color color : {Color::WHITE, Color::BLACK}) {
        const int sign = (color == Color::WHITE) ? 1 : -1;
        for (PieceType type :
             {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
              PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            uint64_t pieces = board.get_bitboard(type, color);
            while (pieces) {
                pieces &= pieces - 1;
                score += sign * values[static_cast<size_t>(type)];
            }
        }
    }
    return score;
}

void test_eval_material_dominates_pst() {
    Board before;
    before.set_from_fen("r3k3/8/8/3N4/8/8/8/4K3 w - - 0 1");
    Board after_fork_win;
    after_fork_win.set_from_fen("N3k3/8/8/8/8/8/8/4K3 w - - 0 1");
    const int material_gain = material_white_perspective(after_fork_win) -
                              material_white_perspective(before);
    const int eval_gain =
        evaluate_board(after_fork_win) - evaluate_board(before);
    unit_check(material_gain == 500,
               "eval balance: capturing rook via fork gains 500 material");
    unit_check(eval_gain > 200,
               "eval balance: material gain still dominates positional terms");
    unit_check(evaluate_board(after_fork_win) > 200,
               "eval balance: up-a-rook end state stays clearly positive");

    Board queen_win;
    queen_win.set_from_fen("4k3/8/8/3q4/8/2N5/8/4K3 w - - 0 1");
    Board after_queen;
    after_queen.set_from_fen("4k3/8/8/3N4/8/8/8/4K3 w - - 0 1");
    unit_check(evaluate_board(after_queen) - evaluate_board(queen_win) > 500,
               "eval balance: winning a free queen stays dominant");
}

void test_positional_evaluation() {
    {
        Board castled;
        castled.set_from_fen(
            "r1bq1rk1/pppp1ppp/2n2n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - "
            "0 8");
        Board exposed;
        exposed.set_from_fen(
            "r1bq1rk1/pppp1ppp/2n2n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQ - "
            "0 8");
        unit_check(evaluate_board(castled) > evaluate_board(exposed),
                   "positional: castled king preferred over uncastled");
    }
    {
        Board shelter;
        shelter.set_from_fen(
            "3q2k1/5ppp/8/8/8/8/5PPP/3Q2K1 w - - 0 1");
        Board broken;
        broken.set_from_fen(
            "3q2k1/5ppp/8/8/8/8/PPP5/3Q2K1 w - - 0 1");
        unit_check(evaluate_board(shelter) > evaluate_board(broken),
                   "positional: intact pawn shelter preferred");
    }
    {
        Board rights;
        rights.set_from_fen(
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
        Board walked;
        walked.set_from_fen(
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPPKPPP/RNBQ1BNR w kq - 0 3");
        unit_check(evaluate_board(rights) > evaluate_board(walked),
                   "positional: early king walk is discouraged");
    }
    {
        Board mid_corner;
        mid_corner.set_from_fen(
            "rnbq1rk1/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQ1RK1 w - - 0 6");
        const int mid_phase = get_game_phase_int(mid_corner);
        Board end_active;
        end_active.set_from_fen("4k3/8/8/3K4/8/8/8/8 w - - 0 1");
        Board end_corner;
        end_corner.set_from_fen("4k3/8/8/8/8/8/8/K7 w - - 0 1");
        unit_check(mid_phase > 12,
                   "positional: middlegame still has high phase");
        unit_check(evaluate_board(end_active) > evaluate_board(end_corner),
                   "positional: active king preferred in endgame");
        unit_check(get_game_phase_int(end_active) == 0,
                   "positional: bare kings are endgame phase 0");
    }
    {
        Board developed;
        developed.set_from_fen(
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
        Board undeveloped;
        undeveloped.set_from_fen(
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
        unit_check(evaluate_board(developed) > evaluate_board(undeveloped),
                   "positional: developing knights is credited");
    }
    {
        Board equal_open;
        equal_open.set_from_fen(
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
        Board up_exchange;
        up_exchange.set_from_fen(
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBN1 w Qkq - 0 2");
        unit_check(evaluate_board(equal_open) - evaluate_board(up_exchange) >
                       300,
                   "positional: missing a rook dominates opening bonuses");
    }
    {
        Board with_queen;
        with_queen.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/3Q2K1 w - - 0 1");
        Board no_queen;
        no_queen.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/6K1 w - - 0 1");
        Board exposed_queen;
        exposed_queen.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/3Q1K2 w - - 0 1");
        Board exposed_quiet;
        exposed_quiet.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/5K2 w - - 0 1");
        const int queen_gap =
            evaluate_board(with_queen) - evaluate_board(exposed_queen);
        const int quiet_gap =
            evaluate_board(no_queen) - evaluate_board(exposed_quiet);
        unit_check(queen_gap > quiet_gap,
                   "positional: king exposure matters more with queens");
    }
}

struct TacticCase {
    const char *name;
    const char *fen;
    const char *expected_move;
    bool expect_avoid;
};

void test_tactics() {
    const TacticCase cases[] = {
        {"hanging rook", "4k3/8/8/8/8/3r4/8/3RK3 w - - 0 1", "d1d3", false},
        {"hanging queen", "4k3/8/8/3q4/8/2N5/8/4K3 w - - 0 1", "c3d5", false},
        {"free minor piece", "4k3/8/8/3n4/2B5/8/8/4K3 w - - 0 1", "c4d5",
         false},
        {"free knight with rook", "4k3/8/8/8/8/8/1n6/1R2K3 w - - 0 1", "b1b2",
         false},
        {"bad exchange sac", "1n2k3/8/2b5/8/8/8/8/2R1K3 w - - 0 1", "c1c6",
         true},
        {"knight fork", "r3k3/8/8/3N4/8/8/8/4K3 w - - 0 1", "d5c7", false},
        {"forced recapture", "4k3/8/3n4/3R4/8/8/8/4K3 w - - 0 1", "d5d6",
         false},
        {"mate in one", "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1", "e1e8", false},
        {"mate in two", "k7/8/1K6/8/8/8/8/1R6 w - - 0 1", "", false},
    };

    for (const TacticCase &tactic : cases) {
        for (int depth : {2, 4, 6}) {
            if (string(tactic.name) == "mate in two" && depth == 2) {
                continue;
            }
            Board board;
            board.set_from_fen(tactic.fen);
            const auto started = chrono::steady_clock::now();
            const SearchResult result = find_best_move(board, depth);
            const auto elapsed =
                chrono::duration_cast<chrono::milliseconds>(
                    chrono::steady_clock::now() - started)
                    .count();
            const string best =
                result.found_move ? move_to_string(result.best_move) : "0000";
            cout << "  tactic " << tactic.name << " d" << depth << " => "
                 << best << " score=" << result.score
                 << " nodes=" << result.nodes << " time=" << elapsed << "ms\n";

            if (string(tactic.name) == "mate in two") {
                unit_check(result.score == MATE_SCORE - 3,
                           ("tactics: mate in two at depth " + to_string(depth))
                               .c_str());
                continue;
            }
            if (string(tactic.name) == "mate in one") {
                unit_check(best == tactic.expected_move &&
                               result.score == MATE_SCORE - 1,
                           ("tactics: mate in one at depth " + to_string(depth))
                               .c_str());
                continue;
            }
            if (tactic.expect_avoid) {
                unit_check(best != tactic.expected_move,
                           ("tactics: avoid " + string(tactic.name) + " at d" +
                            to_string(depth))
                               .c_str());
            } else {
                unit_check(best == tactic.expected_move,
                           ("tactics: " + string(tactic.name) + " at d" +
                            to_string(depth) + " expected " +
                            string(tactic.expected_move) + " got " + best)
                               .c_str());
            }
        }
    }
}

void test_quiescence() {
    {
        Board board;
        board.set_from_fen("r3k3/8/8/3N4/8/8/8/4K3 w - - 0 1");
        const SearchResult shallow = find_best_move(board, 2);
        unit_check(shallow.found_move &&
                       move_to_string(shallow.best_move) == "d5c7",
                   "quiescence: depth 2 finds knight fork (horizon capture)");
        unit_check(shallow.score > 200,
                   "quiescence: fork score reflects winning the rook");
        unit_check(shallow.qnodes > 0,
                   "quiescence: records quiescence nodes on tactical search");
    }
    {
        Board board;
        board.set_from_fen("4k3/8/8/8/8/3r4/8/3RK3 w - - 0 1");
        const SearchResult result = find_best_move(board, 1);
        unit_check(result.found_move &&
                       move_to_string(result.best_move) == "d1d3",
                   "quiescence: depth 1 takes hanging rook at horizon");
    }
    {
        Board board;
        board.set_from_fen("1n2k3/8/2b5/8/8/8/8/2R1K3 w - - 0 1");
        const SearchResult result = find_best_move(board, 2);
        unit_check(result.found_move &&
                       move_to_string(result.best_move) != "c1c6",
                   "quiescence: refuses capture of defended bishop");
    }
    {
        Board board;
        board.set_from_fen("4k3/4r3/8/8/8/8/8/4K3 w - - 0 1");
        const Snapshot before = take_snapshot(board);
        const SearchResult result = find_best_move(board, 2);
        unit_check(result.found_move,
                   "quiescence: check requires a quiet evasion move");
        unit_check(move_to_string(result.best_move) == "e1d1" ||
                       move_to_string(result.best_move) == "e1d2" ||
                       move_to_string(result.best_move) == "e1f1" ||
                       move_to_string(result.best_move) == "e1f2",
                   "quiescence: king steps out of rook check");
        unit_check(snapshot_equal(take_snapshot(board), before),
                   "quiescence: preserves board after check evasion search");
    }
    {
        Board board;
        board.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1");
        const Piece captured = board.get_piece_at(7, 4);
        board.make_move(parse_move_str("e1e8"));
        uint64_t nodes = 0;
        uint64_t qnodes = 0;
        const int score = search_quiescence(board, nodes, qnodes);
        board.undo_move(parse_move_str("e1e8"), captured);
        unit_check(score == -MATE_SCORE,
                   "quiescence: checkmate at horizon returns mate score");
    }
    {
        Board board;
        board.set_from_fen(
            "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 3");
        uint64_t nodes = 0;
        uint64_t qnodes = 0;
        const int score = search_quiescence(board, nodes, qnodes);
        unit_check(score == -MATE_SCORE,
                   "quiescence: already checkmated position scores as mate");
    }
    {
        Board board;
        board.set_from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        uint64_t nodes = 0;
        uint64_t qnodes = 0;
        const int score = search_quiescence(board, nodes, qnodes);
        unit_check(score == 0,
                   "quiescence: stalemate at horizon returns 0");
    }
    {
        Board board;
        board.set_from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        const SearchResult result = find_best_move(board, 1);
        unit_check(result.found_move &&
                       move_to_string(result.best_move) == "e5d6",
                   "quiescence: considers en passant capture");
        uint64_t nodes = 0;
        uint64_t qnodes = 0;
        bool ep_seen = false;
        for (const Move &move : board.generate_all_moves(board.get_color())) {
            if (move_to_string(move) == "e5d6") {
                ep_seen = true;
                const Piece captured =
                    board.get_piece_at(move.end_rank, move.end_file);
                board.make_move(move);
                (void)search_quiescence(board, nodes, qnodes);
                board.undo_move(move, captured);
            }
        }
        unit_check(ep_seen, "quiescence: en passant move is legal in EP test");
    }
    {
        Board board;
        board.set_from_fen("8/P7/8/8/8/8/8/4K2k w - - 0 1");
        const SearchResult result = find_best_move(board, 1);
        unit_check(result.found_move &&
                       move_to_string(result.best_move).rfind("a7a8", 0) == 0,
                   "quiescence: searches promotion moves");
        unit_check(result.score > 500,
                   "quiescence: promotion is strongly preferred");
    }
    {
        Board board;
        board.set_from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        const Snapshot before = take_snapshot(board);
        uint64_t nodes = 0;
        uint64_t qnodes = 0;
        const int score = search_quiescence(board, nodes, qnodes);
        unit_check(snapshot_equal(take_snapshot(board), before),
                   "quiescence: preserves board with no tactical moves");
        unit_check(score == evaluate_board(board),
                   "quiescence: quiet position returns stand-pat eval");
        unit_check(qnodes == 1,
                   "quiescence: quiet leaf counts one quiescence node");
    }
}

void test_search() {
    {
        Board board;
        board.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1");
        const Snapshot before = take_snapshot(board);
        const SearchResult result = find_best_move(board, 1);
        unit_check(result.found_move, "search: mate in 1 finds a move");
        unit_check(move_is_legal(board, result.best_move),
                   "search: mate in 1 move is legal");
        unit_check(delivers_checkmate(board, result.best_move),
                   "search: mate in 1 move delivers checkmate");
        unit_check(result.score == MATE_SCORE - 1,
                   "search: mate in 1 score is MATE_SCORE-1");
        unit_check(snapshot_equal(take_snapshot(board), before),
                   "search: mate in 1 preserves board state");
    }
    {
        Board board;
        board.set_from_fen("k7/8/1K6/8/8/8/8/1R6 w - - 0 1");
        const Snapshot before = take_snapshot(board);
        const SearchResult shallow = find_best_move(board, 1);
        const SearchResult deep = find_best_move(board, 3);
        unit_check(deep.found_move && move_is_legal(board, deep.best_move),
                   "search: mate in 2 finds a legal move at depth 3");
        unit_check(deep.score == MATE_SCORE - 3,
                   "search: mate in 2 score is MATE_SCORE-3 at depth 3");
        unit_check(shallow.score < MATE_SCORE - 1000,
                   "search: depth 1 cannot force the mate in 2");
        unit_check(snapshot_equal(take_snapshot(board), before),
                   "search: mate in 2 preserves board state");
    }
    {
        Board board;
        board.set_from_fen("7k/QQ6/8/8/8/8/8/7K w - - 0 1");
        const SearchResult result = find_best_move(board, 2);
        unit_check(result.found_move && delivers_checkmate(board, result.best_move),
                   "search: prefers a move that mates immediately");
        unit_check(result.score == MATE_SCORE - 1,
                   "search: immediate mate scores higher than delayed mates");
    }
    {
        Board board;
        board.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/4R1K1 b - - 0 1");
        const SearchResult result = find_best_move(board, 2);
        unit_check(result.found_move, "search: avoiding mate finds a move");
        const Piece captured =
            board.get_piece_at(result.best_move.end_rank, result.best_move.end_file);
        board.make_move(result.best_move);
        const SearchResult white_reply = find_best_move(board, 1);
        const bool white_mates =
            white_reply.found_move &&
            delivers_checkmate(board, white_reply.best_move);
        board.undo_move(result.best_move, captured);
        unit_check(!white_mates,
                   "search: avoids allowing an immediate checkmate");
    }
    {
        Board board;
        board.set_from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        const SearchResult result = find_best_move(board, 1);
        unit_check(!result.found_move, "search: stalemate returns no move");
        unit_check(result.score == 0, "search: stalemate score is 0");
    }
    {
        Board checkmate;
        checkmate.set_from_fen(
            "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 3");
        const SearchResult mate_at_root = find_best_move(checkmate, 0);
        unit_check(!mate_at_root.found_move && mate_at_root.score == -MATE_SCORE,
                   "search: checkmate at depth 0 is not a normal eval");

        Board stalemate;
        stalemate.set_from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        const SearchResult stale_at_root = find_best_move(stalemate, 0);
        unit_check(!stale_at_root.found_move && stale_at_root.score == 0,
                   "search: stalemate at depth 0 is not a normal eval");
    }
    {
        Board board;
        board.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1");
        const SearchResult depth1 = find_best_move(board, 1);
        const SearchResult depth3 = find_best_move(board, 3);
        unit_check(depth1.found_move && depth3.found_move,
                   "search: depth 1 and depth 3 both return moves");
        unit_check(delivers_checkmate(board, depth1.best_move) &&
                       delivers_checkmate(board, depth3.best_move),
                   "search: deeper search still finds mate in 1");
        unit_check(depth3.nodes >= depth1.nodes,
                   "search: deeper search visits at least as many nodes");
    }
    {
        Board board;
        board.set_from_fen(
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w "
            "KQkq - 0 1");
        const Snapshot before = take_snapshot(board);
        const SearchResult result = find_best_move(board, 3);
        unit_check(result.found_move && move_is_legal(board, result.best_move),
                   "search: only returns a legal move");
        unit_check(snapshot_equal(take_snapshot(board), before),
                   "search: preserves board after a midgame search");
    }
}

void test_zobrist_and_tt() {
    {
        Board a;
        Board b;
        a.set_from_fen(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        b.set_from_fen(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        unit_check(compute_zobrist_hash(a) == compute_zobrist_hash(b),
                   "zobrist: identical positions share a hash");
    }
    {
        Board white;
        Board black;
        white.set_from_fen(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        black.set_from_fen(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
        unit_check(compute_zobrist_hash(white) != compute_zobrist_hash(black),
                   "zobrist: side to move affects the hash");
    }
    {
        Board with_rights;
        Board without_rights;
        with_rights.set_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        without_rights.set_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1");
        unit_check(compute_zobrist_hash(with_rights) !=
                       compute_zobrist_hash(without_rights),
                   "zobrist: castling rights affect the hash");
    }
    {
        Board with_ep;
        Board without_ep;
        with_ep.set_from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        without_ep.set_from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1");
        unit_check(compute_zobrist_hash(with_ep) !=
                       compute_zobrist_hash(without_ep),
                   "zobrist: legal en passant affects the hash");
    }
    {
        Board board;
        board.set_from_fen(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        const uint64_t before = compute_zobrist_hash(board);
        const Piece captured = board.get_piece_at(3, 4);
        board.make_move(parse_move_str("e2e4"));
        unit_check(compute_zobrist_hash(board) != before,
                   "zobrist: make_move changes the hash");
        board.undo_move(parse_move_str("e2e4"), captured);
        unit_check(compute_zobrist_hash(board) == before,
                   "zobrist: undo_move restores the original hash");
    }
    {
        Board castle;
        castle.set_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        const uint64_t before = compute_zobrist_hash(castle);
        const Piece captured = castle.get_piece_at(0, 6);
        castle.make_move(parse_move_str("e1g1"));
        castle.undo_move(parse_move_str("e1g1"), captured);
        unit_check(compute_zobrist_hash(castle) == before,
                   "zobrist: castling make/undo restores the hash");

        Board promo;
        promo.set_from_fen("8/P7/8/8/8/8/8/4K2k w - - 0 1");
        const uint64_t promo_before = compute_zobrist_hash(promo);
        const Piece promo_captured = promo.get_piece_at(7, 0);
        promo.make_move(parse_move_str("a7a8q"));
        promo.undo_move(parse_move_str("a7a8q"), promo_captured);
        unit_check(compute_zobrist_hash(promo) == promo_before,
                   "zobrist: promotion make/undo restores the hash");

        Board ep;
        ep.set_from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        const uint64_t ep_before = compute_zobrist_hash(ep);
        const Piece ep_captured = ep.get_piece_at(5, 3);
        ep.make_move(parse_move_str("e5d6"));
        ep.undo_move(parse_move_str("e5d6"), ep_captured);
        unit_check(compute_zobrist_hash(ep) == ep_before,
                   "zobrist: en passant make/undo restores the hash");
    }
    {
        Board path_a;
        Board path_b;
        path_a.set_from_fen(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        path_b.set_from_fen(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        path_a.make_move(parse_move_str("e2e4"));
        path_a.make_move(parse_move_str("e7e5"));
        path_a.make_move(parse_move_str("g1f3"));
        path_b.make_move(parse_move_str("g1f3"));
        path_b.make_move(parse_move_str("e7e5"));
        path_b.make_move(parse_move_str("e2e4"));
        unit_check(compute_zobrist_hash(path_a) == compute_zobrist_hash(path_b),
                   "zobrist: transpositions through different orders match");
    }
    {
        Board board;
        board.set_from_fen("6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1");
        clear_search_transposition_table();
        set_search_tt_enabled(false);
        const SearchResult without_tt = find_best_move(board, 3);
        clear_search_transposition_table();
        set_search_tt_enabled(true);
        const SearchResult with_tt = find_best_move(board, 3);
        unit_check(without_tt.found_move && with_tt.found_move &&
                       without_tt.best_move == with_tt.best_move &&
                       without_tt.score == with_tt.score,
                   "tt: mate search matches with TT on or off");
        set_search_tt_enabled(true);
    }
    {
        Board board;
        board.set_from_fen("k7/8/1K6/8/8/8/8/1R6 w - - 0 1");
        clear_search_transposition_table();
        set_search_tt_enabled(false);
        const SearchResult without_tt = find_best_move(board, 4);
        clear_search_transposition_table();
        set_search_tt_enabled(true);
        const SearchResult with_tt = find_best_move(board, 4);
        unit_check(without_tt.score == with_tt.score &&
                       without_tt.score == MATE_SCORE - 3,
                   "tt: mate distance score is preserved with TT");
        set_search_tt_enabled(true);
    }
    {
        Board board;
        board.set_from_fen(
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w "
            "KQkq - 0 1");
        clear_search_transposition_table();
        set_search_tt_enabled(true);
        const Snapshot before = take_snapshot(board);
        const SearchResult cold = find_best_move(board, 4);
        unit_check(snapshot_equal(take_snapshot(board), before),
                   "tt: board preserved after searching");
        const SearchResult warm = find_best_move(board, 4);
        unit_check(cold.found_move && warm.found_move &&
                       cold.best_move == warm.best_move &&
                       cold.score == warm.score,
                   "tt: warm cache repeats the same result");
        unit_check(warm.nodes <= cold.nodes,
                   "tt: warm cache searches no more nodes than cold");
        unit_check(warm.tt_hits > 0, "tt: warm cache records table hits");
    }
    {
        Board board;
        board.set_from_fen("4k3/8/8/3n4/2B5/8/8/4K3 w - - 0 1");
        clear_search_transposition_table();
        set_search_tt_enabled(false);
        const SearchResult without_tt = find_best_move(board, 4);
        clear_search_transposition_table();
        set_search_tt_enabled(true);
        const SearchResult with_tt = find_best_move(board, 4);
        unit_check(without_tt.best_move == with_tt.best_move &&
                       without_tt.score == with_tt.score,
                   "tt: tactical score matches with TT on or off");
        set_search_tt_enabled(true);
    }
}

int run_unit_tests() {
    unit_failures = 0;
    cout << "Unit tests (make/undo state restoration):\n";
    test_ep_undo_double_push();
    test_ep_consecutive_double_pushes();
    test_ep_quiet_move_clears_and_restores();
    test_ep_capture_make_undo();
    test_capture_make_undo();
    test_ep_castling_clears_and_restores();
    test_ep_from_fen();
    test_castling_rights_revoked_on_rook_capture();
    test_castling_rights_revoked_on_rook_and_king_moves();
    test_promotions();
    test_default_board_initialization();
    test_evaluation();
    test_checkmate_and_stalemate();
    test_search();
    test_quiescence();
    test_alpha_beta_matches_reference();
    test_eval_material_dominates_pst();
    test_positional_evaluation();
    test_tactics();
    test_zobrist_and_tt();
    cout << "Unit tests: " << (unit_failures == 0 ? "all passed" : "FAILURES")
         << " (" << unit_failures << " failed)\n\n";
    return unit_failures == 0 ? 0 : 1;
}

bool set_position(Board &board, const Position &pos) {
    if (board.set_from_fen(pos.fen)) {
        return true;
    }
    cout << "  FAIL  could not parse FEN: " << pos.fen << "\n";
    return false;
}

int run_suite(int max_depth, UndoCheck &check) {
    int passed = 0;
    int failed = 0;

    for (const Position &pos : POSITIONS) {
        for (const auto &test : pos.depths) {
            const int depth = test.first;
            const uint64_t expected = test.second;
            if (depth > max_depth) {
                continue;
            }

            cout << pos.name << " depth " << depth << " ... " << flush;

            Board board;
            if (!set_position(board, pos)) {
                ++failed;
                continue;
            }

            // A fresh Board per (position, depth): if one run corrupts
            // state or throws (e.g. Error from generate_legal_moves), the
            // remaining cases still run and report.
            try {
                const uint64_t got =
                    perft(board, depth, board.get_color(), check);
                if (got == expected) {
                    cout << "PASS (" << got << ")\n";
                    ++passed;
                } else {
                    cout << "FAIL (got " << got << ", expected " << expected
                         << ")\n";
                    ++failed;
                }
            } catch (...) {
                cout << "FAIL (exception during perft; board state may be "
                        "corrupt)\n";
                ++failed;
            }
        }
    }

    if (check.enabled) {
        cout << "\nMake/undo round-trip: ";
        if (check.violations == 0) {
            cout << "PASS (no violations)\n";
        } else {
            cout << "FAIL (" << check.violations << " violation(s): "
                 << check.ep_only_violations << " en-passant-only, "
                 << check.board_violations << " board-corrupting; examples "
                 << "of each shown above)\n";
        }
    }

    cout << "\nPerft summary: " << passed << " passed, " << failed
         << " failed\n";
    const bool ok = (failed == 0) &&
                    (!check.enabled || check.violations == 0);
    cout << (ok ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return ok ? 0 : 1;
}

int run_divide(int depth, size_t index, UndoCheck &check) {
    if (index >= POSITIONS.size()) {
        cerr << "divide: position index " << index << " out of range (0-"
             << POSITIONS.size() - 1 << ")\n";
        return 1;
    }
    const Position &pos = POSITIONS[index];
    cout << "divide depth " << depth << "  " << pos.name << "\n  " << pos.fen
         << "\n";

    Board board;
    if (!set_position(board, pos)) {
        return 1;
    }

    const Color side = board.get_color();
    const Color next = (side == Color::WHITE) ? Color::BLACK : Color::WHITE;
    const vector<Move> moves = board.generate_all_moves(side);

    uint64_t total = 0;
    try {
        for (const Move &m : moves) {
            const Piece captured = board.get_piece_at(m.end_rank, m.end_file);
            Snapshot before{};
            if (check.enabled) {
                before = take_snapshot(board);
            }
            board.make_move(m);
            const uint64_t nodes = perft(board, depth - 1, next, check);
            board.undo_move(m, captured);
            if (check.enabled) {
                check_round_trip(board, before, m, check);
            }
            cout << move_to_string(m) << ": " << nodes << "\n";
            total += nodes;
        }
    } catch (...) {
        cout << "FAIL (exception during divide; board state may be "
                "corrupt)\n";
        return 1;
    }

    cout << "\nNodes searched: " << total << "\n";
    return 0;
}

void print_usage(const char *prog) {
    cout << "usage:\n"
         << "  " << prog << "                     run the perft suite\n"
         << "  " << prog << " --max-depth N       only test depths <= N\n"
         << "  " << prog << " --no-check          skip make/undo checks\n"
         << "  " << prog << " --unit              run only the unit tests\n"
         << "  " << prog
         << " --divide D INDEX    per-root-move counts for position INDEX\n"
         << "\npositions:\n";
    for (size_t i = 0; i < POSITIONS.size(); ++i) {
        cout << "  " << i << ": " << POSITIONS[i].name << "\n";
    }
}

} // namespace

int main(int argc, char **argv) {
    int max_depth = 4; // deepest depth listed per position, capped below
    UndoCheck check;
    int divide_depth = -1;
    size_t divide_index = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--max-depth") == 0 && i + 1 < argc) {
            max_depth = stoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-check") == 0) {
            check.enabled = false;
        } else if (strcmp(argv[i], "--unit") == 0) {
            return run_unit_tests();
        } else if (strcmp(argv[i], "--divide") == 0 && i + 2 < argc) {
            divide_depth = stoi(argv[++i]);
            divide_index = static_cast<size_t>(stoul(argv[++i]));
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            cerr << "unknown argument: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (divide_depth >= 0) {
        if (divide_depth == 0) {
            cerr << "divide depth must be at least 1\n";
            return 1;
        }
        return run_divide(divide_depth, divide_index, check);
    }
    const int unit_result = run_unit_tests();
    const int suite_result = run_suite(max_depth, check);
    return (unit_result == 0 && suite_result == 0) ? 0 : 1;
}
