#pragma once

#include <cstdint>

using Bitboard = std::uint64_t;
using Move = std::uint16_t;
using Score = std::int32_t;

enum Colour : std::uint8_t
{WHITE, BLACK, NUM_COLOUR};
constexpr Colour operator~(Colour c)
{return static_cast<Colour>(c ^ 1);}

enum PieceType : std::uint8_t {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NUM_PIECE_TYPE
};

enum Piece : std::uint8_t {     // MAYBE SHOULD DEDICATE 8 FOR WHITE AND 8 FOR BLACK?? LOOK INTO
    W_PAWN,
    W_KNIGHT,
    W_BISHOP,
    W_ROOK,
    W_QUEEN,
    W_KING,

    B_PAWN,
    B_KNIGHT,
    B_BISHOP,
    B_ROOK,
    B_QUEEN,
    B_KING,

    NUM_PIECE,
    NO_PIECE = NUM_PIECE
};

enum Square : std::uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,

    NUM_SQUARE,
    NO_SQUARE = NUM_SQUARE
};

// 16-bit move: to [0..5], from [6..11], promotion [12..13], type [14..15].
constexpr int MOVE_FROM_SHIFT = 6;
constexpr int MOVE_PROMOTION_SHIFT = 12;
constexpr int MOVE_TYPE_SHIFT = 14;
constexpr Move MOVE_SQUARE_MASK = 0x3F;
constexpr Move MOVE_PROMOTION_MASK = 0x3;
constexpr Move MOVE_TYPE_MASK = MOVE_PROMOTION_MASK << MOVE_TYPE_SHIFT;

enum MoveType : Move {
    NORMAL,
    PROMOTION = 1 << MOVE_TYPE_SHIFT,
    EN_PASSANT = 2 << MOVE_TYPE_SHIFT,
    CASTLING = 3 << MOVE_TYPE_SHIFT
};

constexpr Move encodeMove(Square from, Square to, MoveType type = NORMAL,
                          PieceType promotion = KNIGHT) {
    return static_cast<Move>(type |
                             ((promotion - KNIGHT) << MOVE_PROMOTION_SHIFT) |
                             (from << MOVE_FROM_SHIFT) | to);
}

constexpr Square moveFrom(Move move) {
    return static_cast<Square>((move >> MOVE_FROM_SHIFT) & MOVE_SQUARE_MASK);
}

constexpr Square moveTo(Move move) {
    return static_cast<Square>(move & MOVE_SQUARE_MASK);
}

constexpr MoveType moveType(Move move) {
    return static_cast<MoveType>(move & MOVE_TYPE_MASK);
}

constexpr PieceType promotionType(Move move) {
    return static_cast<PieceType>(
        ((move >> MOVE_PROMOTION_SHIFT) & MOVE_PROMOTION_MASK) + KNIGHT);
}

enum File : std::uint8_t {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
    NUM_FILE
};

constexpr Bitboard BB_FILE_A = 0x0101'0101'0101'0101ULL;
constexpr Bitboard BB_FILE_H = 0x8080'8080'8080'8080ULL;

enum Rank : std::uint8_t {
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
    NUM_RANK
};

constexpr Bitboard BB_RANK_1 = 0x0000'0000'0000'00FFULL;
constexpr Bitboard BB_RANK_8 = 0xFF00'0000'0000'0000ULL;


constexpr File fileof(Square sq){
    return static_cast<File>(sq & 7);
}

constexpr Rank rankof(Square sq){
    return static_cast<Rank>(sq >> 3);
}

constexpr Square makeSquare(File file, Rank rank) {
    return static_cast<Square>(
        (static_cast<int>(rank) << 3) +
        static_cast<int>(file)
    );
}

constexpr Bitboard squareBit(Square sq) {
    return Bitboard{1} << sq;
}

constexpr Colour getColour(Piece p){
    return static_cast<Colour>(p/NUM_PIECE_TYPE);
}

constexpr PieceType getPieceType(Piece p){
    return static_cast<PieceType>(p%NUM_PIECE_TYPE);
}
