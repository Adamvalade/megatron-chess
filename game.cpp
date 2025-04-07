// #include <iostream>      // std::cout, std::cin
// #include <map>           // std::map for the transposition table
// #include <string>        // std::string
// #include <climits>       // INT_MIN, INT_MAX
// #include <vector>        // std::vector
// #include <cassert>       // assert
// #include "game.h"
// #include "board.h"       // Your board, piece, move, color, etc.


// static int simple_evaluate(Board &board) 
//  {
//      // We'll just do a quick piece count:
//      //   (Simple approach: assume each piece has a "value"; sum for White minus sum for Black.)
//      // If you already have a good evaluate_board, just call it here.
 
//      // Example piece values
//      auto piece_value = [](PieceType pt) {
//          switch(pt) {
//              case PieceType::PAWN:   return 100;
//              case PieceType::KNIGHT: return 320;
//              case PieceType::BISHOP: return 330;
//              case PieceType::ROOK:   return 500;
//              case PieceType::QUEEN:  return 900;
//              case PieceType::KING:   return 20000;
//              default:                return 0;
//          }
//      };
 
//      int score = 0;
//      // We interpret board.get_color() as "side to move," but for a simpler approach,
//      // let's just do White minus Black. 
//      // If you want a side-to-move perspective, adapt accordingly.
//      for (int r = 0; r < 8; ++r) {
//          for (int f = 0; f < 8; ++f) {
//              const Piece &p = board.get_piece_at(r, f);
//              if (p.getType() == PieceType::NONE) {
//                  continue;
//              }
//              int val = piece_value(p.getType());
//              if (p.getColor() == Color::WHITE) {
//                  score += val;
//              } else if (p.getColor() == Color::BLACK) {
//                  score -= val;
//              }
//          }
//      }
//      return score;
//  }
 
//  /***************************************************************************
//   * Transposition-table entry
//   ***************************************************************************/
//  struct TTEntry {
//      int value;
//      int depth;
//      int flag; // EXACT=0, LOWERBOUND=1, UPPERBOUND=2
//  };
 
//  enum { TT_EXACT = 0, TT_LOWERBOUND = 1, TT_UPPERBOUND = 2 };
 
//  /***************************************************************************
//   * Convert BoardState to a string so we can store it as a map key
//   ***************************************************************************/
//  static std::string boardStateToString(const BoardState &st) 
//  {
//      // Example: combine all bitboards and relevant state into one string
//      // Adjust to your BoardState fields
//      // This is a simple demonstration:
//      // 
//      // white_pawns, white_knights, ..., black_kings, castlingRights, enPassantSquare
//      //
//      // We'll just separate them with commas. No fancy formatting needed.
 
//      // Because we don't want warnings about string + int, we'll use std::to_string.
//      std::string result;
//      result += std::to_string(st.white_pawns)    + ",";
//      result += std::to_string(st.white_knights)  + ",";
//      result += std::to_string(st.white_bishops)  + ",";
//      result += std::to_string(st.white_rooks)    + ",";
//      result += std::to_string(st.white_queens)   + ",";
//      result += std::to_string(st.white_kings)    + ",";
//      result += std::to_string(st.black_pawns)    + ",";
//      result += std::to_string(st.black_knights)  + ",";
//      result += std::to_string(st.black_bishops)  + ",";
//      result += std::to_string(st.black_rooks)    + ",";
//      result += std::to_string(st.black_queens)   + ",";
//      result += std::to_string(st.black_kings)    + ",";
//     //  result += std::to_string(static_cast<int>(st.castlingRights)) + ",";
//     //  result += std::to_string(static_cast<int>(st.enPassantSquare));
//      return result;
//  }
 
//  /***************************************************************************
//   * Engine: alpha–beta search with a map-based transposition table
//   ***************************************************************************/
//  class Engine {
//  public:
//      Board *board;
//      int maxDepth;
//      Move bestMove;
 
//      // map<BoardState,TTEntry> would require a custom operator< for BoardState,
//      // or a custom comparator. Instead, we store std::string keys:
//      std::map<std::string, TTEntry> tt; 
 
//      Engine(Board *b, int depth)
//          : board(b), maxDepth(depth)
//      {
//          // no extra init
//      }
 
//      // Main entry: pick and make a move
//      Move make_move() 
//      {
//          bestMove = Move();
//          // Iterative deepening approach:
//          for (int depth = 1; depth <= maxDepth; ++depth) {
//              (void) search(depth, /*isMax*/true, INT_MIN, INT_MAX);
//          }
//          // Actually make the bestMove on the board
//          board->make_move(bestMove);
//          return bestMove;
//      }
 
//  private:
//      // The alpha–beta search
//      int search(int depth, bool isMax, int alpha, int beta)
//      {
//          // Check the transposition table
//          BoardState st = board->get_current_board_state();
//          std::string key = boardStateToString(st);
 
//          auto it = tt.find(key);
//          if (it != tt.end()) {
//              const TTEntry &entry = it->second;
//              if (entry.depth >= depth) {
//                  // Use the stored value
//                  if (entry.flag == TT_EXACT) {
//                      return entry.value;
//                  } else if (entry.flag == TT_LOWERBOUND) {
//                      if (entry.value > alpha) {
//                          alpha = entry.value;
//                      }
//                  } else if (entry.flag == TT_UPPERBOUND) {
//                      if (entry.value < beta) {
//                          beta = entry.value;
//                      }
//                  }
//                  if (alpha >= beta) {
//                      return entry.value;
//                  }
//              }
//          }
 
//          // If depth=0 or game over, evaluate
//          if (depth == 0 || board->is_checkmate(board->get_color()) || board->is_stalemate(board->get_color())) {
//              int val = simple_evaluate(*board);
//              storeTT(key, val, depth, TT_EXACT);
//              return val;
//          }
 
//          // Generate moves
//          Color side = board->get_color();
//          std::vector<Move> moves = board->generate_all_moves(side);
//          if (moves.empty()) {
//              // No moves => either stalemate or checkmate
//              // If checkmate -> is_checkmate is already checked above, so treat as 0
//              int val = 0;
//              storeTT(key, val, depth, TT_EXACT);
//              return val;
//          }
 
//          int bestEval = isMax ? INT_MIN : INT_MAX;
//         //  Move localBest = moves[0];
 
//          for (const Move &m : moves) {
//              // We'll do the typical alpha–beta
//              Piece captured = board->get_piece_at(m.end_rank, m.end_file);
//              board->make_move(m);
 
//              int eval = search(depth - 1, !isMax, alpha, beta);
 
//              board->undo_move(m, captured);
 
//              if (isMax) {
//                  if (eval > bestEval) {
//                      bestEval = eval;
//                      if (depth == maxDepth) {
//                          bestMove = m; // store globally at the top level
//                      }
//                  }
//                  if (bestEval > alpha) {
//                      alpha = bestEval;
//                  }
//              } else {
//                  if (eval < bestEval) {
//                      bestEval = eval;
//                      if (depth == maxDepth) {
//                          bestMove = m;
//                      }
//                  }
//                  if (bestEval < beta) {
//                      beta = bestEval;
//                  }
//              }
 
//              if (beta <= alpha) {
//                  break; // alpha–beta cutoff
//              }
//          }
 
//          // Decide how to store in TT
//          int flag = TT_EXACT;
//          if (bestEval <= alpha) {
//              flag = TT_UPPERBOUND;
//          } else if (bestEval >= beta) {
//              flag = TT_LOWERBOUND;
//          }
//          storeTT(key, bestEval, depth, flag);
 
//          return bestEval;
//      }
 
//      void storeTT(const std::string &key, int value, int depth, int flag)
//      {
//          TTEntry entry;
//          entry.value = value;
//          entry.depth = depth;
//          entry.flag  = flag;
//          tt[key] = entry;
//      }
//  };
 
//  /***************************************************************************
//   * Game class: orchestrates playing as White & Black
//   ***************************************************************************/
//  class Game {
//     public:
//         Board board;
//         Engine *whiteEngine;
//         Engine *blackEngine;
    
//         // If true => that side is an engine, else it's a human
//         bool whiteIsEngine;
//         bool blackIsEngine;
    
//         Game()
//           : board(),
//             whiteEngine(nullptr),
//             blackEngine(nullptr),
//             whiteIsEngine(true),
//             blackIsEngine(true)
//         {
//             // e.g. create engines with a certain depth
//             whiteEngine = new Engine(&board, 5);
//             blackEngine = new Engine(&board, 5);
//         }
    
//         ~Game() 
//         {
//             delete whiteEngine;
//             delete blackEngine;
//         }
    
//         void go()
//         {
//             board.display();
    
//             // up to 100 plies for demonstration
//             for (int ply = 0; ply < 100; ++ply) {
//                 // check game over
//                 Color side = board.get_color();
//                 if (board.is_checkmate(side)) {
//                     std::cout << ((side == Color::WHITE) ? "Black" : "White") 
//                               << " wins by checkmate!\n";
//                     return;
//                 }
//                 if (board.is_stalemate(side)) {
//                     std::cout << "Stalemate. It's a draw!\n";
//                     return;
//                 }
    
//                 if (side == Color::WHITE && whiteIsEngine) {
//                     whiteEngine->make_move();
//                 } else if (side == Color::BLACK && blackIsEngine) {
//                     blackEngine->make_move();
//                 } else {
//                     // Human turn
//                     Move m = read_user_move();
//                     board.make_move(m); 
//                 }
    
//                 board.display();
//             }
//             std::cout << "Reached move limit.\n";
//         }
    
//     private:
//         // Minimal user-input
//         Move read_user_move()
//         {
//             std::cout << "Enter move (e.g. e2e4): ";
//             std::string in;
//             std::cin >> in;
//             if (in.size() != 4) {
//                 // fallback
//                 return Move{0,1,0,3};
//             }
//             // parse
//             int sf = in[0] - 'a';
//             int sr = in[1] - '1';
//             int ef = in[2] - 'a';
//             int er = in[3] - '1';
//             return Move{sf, sr, ef, er};
//         }
//  };

 