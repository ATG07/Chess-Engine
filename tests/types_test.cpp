#include "types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

static_assert(std::is_same_v<Bitboard, std::uint64_t>);
static_assert(std::is_same_v<Move, std::uint16_t>);
static_assert(std::is_same_v<Score, std::int32_t>);
static_assert(sizeof(Colour) == sizeof(std::uint8_t));
static_assert(sizeof(PieceType) == sizeof(std::uint8_t));
static_assert(sizeof(Piece) == sizeof(std::uint8_t));
static_assert(sizeof(Square) == sizeof(std::uint8_t));

TEST(TypesTest, OppositeColourIsAnInvolution) {
    EXPECT_EQ(~WHITE, BLACK);
    EXPECT_EQ(~BLACK, WHITE);
    EXPECT_EQ(~~WHITE, WHITE);
    EXPECT_EQ(~~BLACK, BLACK);
}

TEST(TypesTest, MoveEncodingRoundTripsNormalAndSpecialMoves) {
    const Move normal = encodeMove(E2, E4);
    EXPECT_EQ(normal, static_cast<Move>((E2 << 6) | E4));
    EXPECT_EQ(moveFrom(normal), E2);
    EXPECT_EQ(moveTo(normal), E4);
    EXPECT_EQ(moveType(normal), NORMAL);

    for (int piece = KNIGHT; piece <= QUEEN; ++piece) {
        const Move promotion = encodeMove(A7, A8, PROMOTION,
                                          static_cast<PieceType>(piece));
        EXPECT_EQ(moveFrom(promotion), A7);
        EXPECT_EQ(moveTo(promotion), A8);
        EXPECT_EQ(moveType(promotion), PROMOTION);
        EXPECT_EQ(promotionType(promotion), static_cast<PieceType>(piece));
    }

    EXPECT_EQ(moveType(encodeMove(E5, D6, EN_PASSANT)), EN_PASSANT);
    const Move castling = encodeMove(E1, H1, CASTLING);
    EXPECT_EQ(moveType(castling), CASTLING);
    EXPECT_EQ(moveFrom(castling), E1);
    EXPECT_EQ(moveTo(castling), H1);  // Stockfish stores the rook square.
}

TEST(TypesTest, PieceValuesAreGroupedByColourAndType) {
    for (int type = PAWN; type < NUM_PIECE_TYPE; ++type) {
        EXPECT_EQ(static_cast<int>(W_PAWN) + type, type);
        EXPECT_EQ(static_cast<int>(B_PAWN) + type,
                  static_cast<int>(NUM_PIECE_TYPE) + type);
    }

    EXPECT_EQ(NUM_PIECE, 12);
    EXPECT_EQ(NO_PIECE, NUM_PIECE);
}

TEST(TypesTest, EverySquareRoundTripsThroughFileAndRank) {
    for (int rank = RANK_1; rank < NUM_RANK; ++rank) {
        for (int file = FILE_A; file < NUM_FILE; ++file) {
            const auto square = makeSquare(static_cast<File>(file),
                                           static_cast<Rank>(rank));
            EXPECT_EQ(fileof(square), static_cast<File>(file));
            EXPECT_EQ(rankof(square), static_cast<Rank>(rank));
            EXPECT_EQ(static_cast<int>(square), rank * 8 + file);
        }
    }
}

TEST(TypesTest, SquareBitSetsExactlyTheRequestedBit) {
    Bitboard allSquares = 0;

    for (int square = A1; square < NUM_SQUARE; ++square) {
        const Bitboard bit = squareBit(static_cast<Square>(square));
        EXPECT_NE(bit, 0U);
        EXPECT_EQ(bit & (bit - 1), 0U);
        EXPECT_EQ(bit, Bitboard{1} << square);
        allSquares |= bit;
    }

    EXPECT_EQ(allSquares, ~Bitboard{0});
}
