#include "uci.h"

#include "board.h"
#include "search.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {

constexpr const char *START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int DEFAULT_SEARCH_DEPTH = 4;

void reply(const string &text) {
    cout << text << '\n';
    cout.flush();
}

string move_to_uci(const Move &move) {
    string text;
    text += static_cast<char>('a' + move.start_file);
    text += static_cast<char>('1' + move.start_rank);
    text += static_cast<char>('a' + move.end_file);
    text += static_cast<char>('1' + move.end_rank);
    switch (move.promotion) {
    case PieceType::QUEEN:
        text += 'q';
        break;
    case PieceType::ROOK:
        text += 'r';
        break;
    case PieceType::BISHOP:
        text += 'b';
        break;
    case PieceType::KNIGHT:
        text += 'n';
        break;
    default:
        break;
    }
    return text;
}

bool parse_uci_move(const string &text, Move &move) {
    if (text.size() < 4 || text.size() > 5) {
        return false;
    }
    if (text[0] < 'a' || text[0] > 'h' || text[1] < '1' || text[1] > '8' ||
        text[2] < 'a' || text[2] > 'h' || text[3] < '1' || text[3] > '8') {
        return false;
    }

    move.start_file = text[0] - 'a';
    move.start_rank = text[1] - '1';
    move.end_file = text[2] - 'a';
    move.end_rank = text[3] - '1';
    move.promotion = PieceType::NONE;

    if (text.size() == 5) {
        switch (static_cast<char>(tolower(static_cast<unsigned char>(text[4])))) {
        case 'q':
            move.promotion = PieceType::QUEEN;
            break;
        case 'r':
            move.promotion = PieceType::ROOK;
            break;
        case 'b':
            move.promotion = PieceType::BISHOP;
            break;
        case 'n':
            move.promotion = PieceType::KNIGHT;
            break;
        default:
            return false;
        }
    }

    return true;
}

bool apply_uci_move(Board &board, const string &text) {
    Move requested;
    if (!parse_uci_move(text, requested)) {
        cerr << "info string invalid move format: " << text << '\n';
        return false;
    }

    const vector<Move> legal = board.generate_all_moves(board.get_color());
    for (const Move &candidate : legal) {
        if (candidate == requested) {
            board.make_move(candidate);
            return true;
        }
    }

    cerr << "info string illegal move: " << text << '\n';
    return false;
}

bool load_fen_from_tokens(Board &board, istringstream &tokens) {
    vector<string> parts;
    string token;
    bool saw_moves = false;
    while (parts.size() < 6 && tokens >> token) {
        if (token == "moves") {
            saw_moves = true;
            break;
        }
        parts.push_back(token);
    }

    if (parts.size() < 4) {
        return false;
    }
    if (parts.size() == 4) {
        parts.push_back("0");
        parts.push_back("1");
    } else if (parts.size() == 5) {
        parts.push_back("1");
    }

    string fen = parts[0];
    for (size_t i = 1; i < 6; ++i) {
        fen += ' ';
        fen += parts[i];
    }

    if (!board.set_from_fen(fen)) {
        return false;
    }

    if (saw_moves) {
        return true;
    }
    if (tokens >> token) {
        return token == "moves";
    }
    return true;
}

void apply_move_list(Board &board, istringstream &tokens) {
    string move_text;
    while (tokens >> move_text) {
        if (!apply_uci_move(board, move_text)) {
            return;
        }
    }
}

void handle_position(Board &board, istringstream &tokens) {
    string kind;
    if (!(tokens >> kind)) {
        cerr << "info string missing position argument\n";
        return;
    }

    if (kind == "startpos") {
        if (!board.set_from_fen(START_FEN)) {
            cerr << "info string failed to load startpos\n";
            return;
        }
        string maybe_moves;
        if (tokens >> maybe_moves) {
            if (maybe_moves != "moves") {
                cerr << "info string expected moves after startpos\n";
                return;
            }
            apply_move_list(board, tokens);
        }
        return;
    }

    if (kind == "fen") {
        if (!load_fen_from_tokens(board, tokens)) {
            cerr << "info string failed to parse fen\n";
            return;
        }
        apply_move_list(board, tokens);
        return;
    }

    cerr << "info string unknown position kind: " << kind << '\n';
}

void handle_go(Board &board, istringstream &tokens) {
    int depth = DEFAULT_SEARCH_DEPTH;
    string token;
    while (tokens >> token) {
        if (token == "depth") {
            int requested = DEFAULT_SEARCH_DEPTH;
            if (tokens >> requested) {
                if (requested < 1) {
                    requested = 1;
                }
                depth = requested;
            }
        }
    }

    const SearchResult result = find_best_move(board, depth);
    reply("info depth " + to_string(depth) + " score cp " +
          to_string(result.score) + " nodes " + to_string(result.nodes));
    cerr << "info string qnodes " << result.qnodes << " tt_probes "
         << result.tt_probes << " tt_hits " << result.tt_hits
         << " tt_cutoffs " << result.tt_cutoffs << '\n';
    if (!result.found_move) {
        reply("bestmove 0000");
        return;
    }
    reply("bestmove " + move_to_uci(result.best_move));
}

} // namespace

void reset_to_startpos(Board &board) {
    if (!board.set_from_fen(START_FEN)) {
        board.clear();
        board.generate_board();
    }
}

void run_uci() {
    Board board;
    reset_to_startpos(board);

    string line;
    while (getline(cin, line)) {
        istringstream tokens(line);
        string command;
        if (!(tokens >> command)) {
            continue;
        }

        if (command == "uci") {
            reply("id name Megatron Chess");
            reply("id author Adam Valade");
            reply("uciok");
        } else if (command == "isready") {
            reply("readyok");
        } else if (command == "ucinewgame") {
            reset_to_startpos(board);
            clear_search_transposition_table();
        } else if (command == "position") {
            handle_position(board, tokens);
        } else if (command == "go") {
            handle_go(board, tokens);
        } else if (command == "stop") {
        } else if (command == "quit") {
            break;
        }
    }
}
