#include <iostream>
#include "Piece.h"

ostream & operator<<(ostream & os, const Piece & piece) {

    if (piece.getType() == PieceType::NONE || piece.getColor() == Color::NONE) 
        os << " ";
    if (piece.getColor() == Color::WHITE && piece.getType() == PieceType::PAWN)
        os << "P";
    else if (piece.getColor() == Color::WHITE && piece.getType() == PieceType::KNIGHT)
        os << "K";
    else if (piece.getColor() == Color::WHITE && piece.getType() == PieceType::BISHOP)
        os << "B";
    else if (piece.getColor() == Color::WHITE && piece.getType() == PieceType::ROOK)
        os << "R";
    else if (piece.getColor() == Color::WHITE && piece.getType() == PieceType::QUEEN)
        os << "Q";
    else if (piece.getColor() == Color::WHITE && piece.getType() == PieceType::KING)
        os << "X";
    else if (piece.getType() == PieceType::PAWN)
        os << "p";
    else if (piece.getType() == PieceType::KNIGHT)
        os << "k";
    else if (piece.getType() == PieceType::BISHOP)
        os << "b";
    else if (piece.getType() == PieceType::ROOK)
        os << "r";
    else if (piece.getType() == PieceType::QUEEN)
        os << "q";
    else if (piece.getType() == PieceType::KING)
        os << "x";

    return os;
}

string Piece::return_string() {

    if (getType() == PieceType::PAWN && getColor() == Color::WHITE)
        return "P";
    else if (getType() == PieceType::PAWN)
        return "p";
    else if (getType() == PieceType::KNIGHT && getColor() == Color::WHITE)
        return "N";
    else if (getType() == PieceType::KNIGHT)
        return "n";
    else if (getType() == PieceType::BISHOP && getColor() == Color::WHITE)
        return "B";
    else if (getType() == PieceType::BISHOP)
        return "b";
    else if (getType() == PieceType::ROOK && getColor() == Color::WHITE)
        return "R";
    else if (getType() == PieceType::ROOK)
        return "r";
    else if (getType() == PieceType::KING && getColor() == Color::WHITE)
        return "K";
    else if (getType() == PieceType::KING)
        return "k";
    else if (getType() == PieceType::QUEEN && getColor() == Color::WHITE)
        return "Q";
    else if (getType() == PieceType::QUEEN)
        return "q";
    else if (getType() == PieceType::NONE)
        return "";

    else {
        assert(false);
        return "a";
    }
}
