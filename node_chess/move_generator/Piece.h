#ifndef PIECE_H
#define PIECE_H

#include <iostream>
#include <cassert>

using namespace std;


enum class Color {
    WHITE,
    BLACK,
    NONE
};

enum class PieceType {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NONE
};

class Piece {

    public:

        Piece(PieceType type = PieceType::NONE, Color color = Color::NONE, size_t position = 64)
            : type(type), color(color), position(position) {}

        
        PieceType getType() const { return type; }
        Color getColor() const { return color; }
        size_t getPosition() const { return position; }
        string return_string();

       
        void setType(PieceType t) { type = t; }
        void setColor(Color c) { color = c; }
        void setPosition(int p) { position = static_cast<size_t>(p); }
        
        friend std::ostream& operator<<(std::ostream & os, const Piece & piece);

    private:

        PieceType type;
        Color color;
        size_t position; 
};


#endif 