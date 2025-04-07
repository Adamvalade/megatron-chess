#!/usr/bin/env python3
import chess
import chess.svg

def generate_positions(depth, board=None):
    if board is None:
        board = chess.Board()

    if depth == 0:
        # Print the current position (piece placement and active color only)
        fen_parts = board.fen().split(" ")
        print(f"{fen_parts[0]} {fen_parts[1]}")
        return

    for move in board.legal_moves:
        board.push(move)
        generate_positions(depth - 1, board)
        board.pop()

# Generate all positions after 5 plies and print to stdout
# generate_positions(5)


def print_fen(pos):
    board = chess.Board(pos)
    svg_data = chess.svg.board(board=board)
    # Save the SVG to a file
    with open("chessboard.svg", "w") as file:
        file.write(svg_data)
    print("SVG saved as 'chessboard.svg'. Open it in your browser to view the position.")

def main():
    generate_positions(7)
    # print_fen("rnb1kbnr/pp1ppppp/8/B1p5/3P4/8/PPP1PPPP/RN1QKBNR")

main()