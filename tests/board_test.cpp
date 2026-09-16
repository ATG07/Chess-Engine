#include "board.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <iterator>

namespace {

struct PublicBoardState {
    std::array<Piece, NUM_SQUARE> mailbox{};
    std::array<std::array<Bitboard, NUM_PIECE_TYPE>, NUM_COLOUR> pieces{};
    std::array<Bitboard, NUM_COLOUR> occupancy{};
    Bitboard occupancyAll = 0;
    Colour sideToMove = WHITE;
    std::uint8_t castlingRights = 0;
    bool hasEnPassant = false;
    File enPassantFile = FILE_A;
    Square enPassantSquare = NO_SQUARE;
    std::uint16_t halfmoveClock = 0;
    std::uint16_t fullmoveNumber = 1;
    std::uint64_t hash = 0;
};

PublicBoardState publicState(const Board& board) {
    PublicBoardState state;
    for (int square = A1; square < NUM_SQUARE; ++square) {
        state.mailbox[square] = board.pieceAt(static_cast<Square>(square));
    }
    for (int colour = WHITE; colour < NUM_COLOUR; ++colour) {
        state.occupancy[colour] = board.occupancy(static_cast<Colour>(colour));
        for (int type = PAWN; type < NUM_PIECE_TYPE; ++type) {
            state.pieces[colour][type] =
                board.pieces(static_cast<Colour>(colour),
                             static_cast<PieceType>(type));
        }
    }
    state.occupancyAll = board.occupancy();
    state.sideToMove = board.sideToMove();
    state.castlingRights = board.castlingRights();
    state.hasEnPassant = board.hasEnPassant();
    state.enPassantFile = board.enPassantFile();
    state.enPassantSquare = board.enPassantSquare();
    state.halfmoveClock = board.halfmoveClock();
    state.fullmoveNumber = board.fullmoveNumber();
    state.hash = board.hash();
    return state;
}

void expectSameState(const PublicBoardState& expected, const Board& actual) {
    const auto observed = publicState(actual);
    EXPECT_EQ(observed.mailbox, expected.mailbox);
    EXPECT_EQ(observed.pieces, expected.pieces);
    EXPECT_EQ(observed.occupancy, expected.occupancy);
    EXPECT_EQ(observed.occupancyAll, expected.occupancyAll);
    EXPECT_EQ(observed.sideToMove, expected.sideToMove);
    EXPECT_EQ(observed.castlingRights, expected.castlingRights);
    EXPECT_EQ(observed.hasEnPassant, expected.hasEnPassant);
    EXPECT_EQ(observed.enPassantFile, expected.enPassantFile);
    EXPECT_EQ(observed.enPassantSquare, expected.enPassantSquare);
    EXPECT_EQ(observed.halfmoveClock, expected.halfmoveClock);
    EXPECT_EQ(observed.fullmoveNumber, expected.fullmoveNumber);
    EXPECT_EQ(observed.hash, expected.hash);
}

}  // namespace

TEST(PackedStateTest, PacksAndUnpacksEveryField) {
    constexpr std::uint8_t rights =
        WHITE_KINGSIDE | BLACK_KINGSIDE | BLACK_QUEENSIDE;
    constexpr PackedState state = makePackedState(rights, true, FILE_F);

    EXPECT_EQ(castlingRights(state), rights);
    EXPECT_TRUE(hasEnPassant(state));
    EXPECT_EQ(enPassantFile(state), FILE_F);
}

TEST(PackedStateTest, MasksUnknownCastlingBitsAndOmitsUnusedEpFile) {
    constexpr PackedState state = makePackedState(0xFF, false, FILE_H);

    EXPECT_EQ(castlingRights(state), CASTLING_MASK);
    EXPECT_FALSE(hasEnPassant(state));
    EXPECT_EQ(state & EP_FILE_MASK, 0);
}

TEST(MoveListTest, SupportsInsertionIterationIndexingAndClear) {
    MoveList moves;
    const Move first = encodeMove(E2, E4);
    const Move second = encodeMove(G1, F3);
    const Move third = encodeMove(A7, A8, PROMOTION, QUEEN);
    moves.add(first);
    moves.add(second);
    moves.add(third);

    ASSERT_EQ(moves.size, 3);
    EXPECT_EQ(moves[1], second);
    moves[1] = encodeMove(B1, C3);

    const MoveList& constMoves = moves;
    EXPECT_EQ(moveFrom(constMoves[1]), B1);
    EXPECT_EQ(moveTo(constMoves[1]), C3);
    EXPECT_EQ(std::distance(constMoves.begin(), constMoves.end()), 3);
    const std::array<Move, 3> actual{moves[0], moves[1], moves[2]};
    const std::array<Move, 3> expected{
        first, encodeMove(B1, C3), third};
    EXPECT_EQ(actual, expected);

    moves.clear();
    EXPECT_EQ(moves.size, 0);
    EXPECT_EQ(moves.begin(), moves.end());
}

TEST(BoardTest, StartingPositionHasExpectedMailboxAndBitboards) {
    Board board;
    board.setStartingPosition();

    EXPECT_EQ(board.sideToMove(), WHITE);
    EXPECT_EQ(board.castlingRights(),
              WHITE_KINGSIDE | WHITE_QUEENSIDE |
                  BLACK_KINGSIDE | BLACK_QUEENSIDE);
    EXPECT_FALSE(board.hasEnPassant());
    EXPECT_EQ(board.enPassantSquare(), NO_SQUARE);
    EXPECT_EQ(board.halfmoveClock(), 0);
    EXPECT_EQ(board.fullmoveNumber(), 1);

    EXPECT_EQ(board.pieces(WHITE, PAWN), 0x000000000000FF00ULL);
    EXPECT_EQ(board.pieces(BLACK, PAWN), 0x00FF000000000000ULL);
    EXPECT_EQ(board.occupancy(WHITE), 0x000000000000FFFFULL);
    EXPECT_EQ(board.occupancy(BLACK), 0xFFFF000000000000ULL);
    EXPECT_EQ(board.occupancy(), 0xFFFF00000000FFFFULL);

    EXPECT_EQ(board.pieceAt(A1), W_ROOK);
    EXPECT_EQ(board.pieceAt(E1), W_KING);
    EXPECT_EQ(board.pieceAt(D8), B_QUEEN);
    EXPECT_EQ(board.pieceAt(H8), B_ROOK);
    EXPECT_EQ(board.pieceAt(E4), NO_PIECE);
}

TEST(BoardTest, FenPopulatesPositionAndAllMetadata) {
    Board board;
    board.setFEN(
        "r3k2r/ppp2ppp/2npbn2/3qp3/3P4/2N1PN2/PPP2PPP/R2QKB1R "
        "b KQkq e3 17 42");

    EXPECT_EQ(board.sideToMove(), BLACK);
    EXPECT_EQ(board.castlingRights(),
              WHITE_KINGSIDE | WHITE_QUEENSIDE |
                  BLACK_KINGSIDE | BLACK_QUEENSIDE);
    EXPECT_TRUE(board.hasEnPassant());
    EXPECT_EQ(board.enPassantFile(), FILE_E);
    EXPECT_EQ(board.enPassantSquare(), E3);
    EXPECT_EQ(board.halfmoveClock(), 17);
    EXPECT_EQ(board.fullmoveNumber(), 42);

    EXPECT_EQ(board.pieceAt(A8), B_ROOK);
    EXPECT_EQ(board.pieceAt(E8), B_KING);
    EXPECT_EQ(board.pieceAt(D5), B_QUEEN);
    EXPECT_EQ(board.pieceAt(D4), W_PAWN);
    EXPECT_EQ(board.pieceAt(C3), W_KNIGHT);
    EXPECT_EQ(board.pieceAt(E2), NO_PIECE);
    EXPECT_EQ(board.pieceAt(E1), W_KING);
}

TEST(BoardTest, SettingFenReplacesThePreviousPosition) {
    Board board;
    board.setStartingPosition();
    board.setFEN("4k3/8/8/8/8/8/8/4K3 w - - 3 9");

    EXPECT_EQ(board.occupancy(WHITE), squareBit(E1));
    EXPECT_EQ(board.occupancy(BLACK), squareBit(E8));
    EXPECT_EQ(board.occupancy(), squareBit(E1) | squareBit(E8));
    EXPECT_EQ(board.pieceAt(A1), NO_PIECE);
    EXPECT_EQ(board.pieceAt(E1), W_KING);
    EXPECT_EQ(board.pieceAt(E8), B_KING);
    EXPECT_EQ(board.castlingRights(), 0);
    EXPECT_FALSE(board.hasEnPassant());
    EXPECT_EQ(board.enPassantSquare(), NO_SQUARE);
    EXPECT_EQ(board.halfmoveClock(), 3);
    EXPECT_EQ(board.fullmoveNumber(), 9);
}

TEST(BoardTest, AttackDetectionHandlesLeapersAndSliders) {
    Board board;
    board.setFEN("4k3/8/2n5/3p4/8/8/4r3/4K3 w - - 0 1");

    EXPECT_TRUE(board.isSquareAttacked(E1, BLACK));  // rook
    EXPECT_TRUE(board.isSquareAttacked(E4, BLACK));  // pawn from d5
    EXPECT_TRUE(board.isSquareAttacked(E5, BLACK));  // knight from c6
    EXPECT_FALSE(board.isSquareAttacked(D4, BLACK));
    EXPECT_TRUE(board.inCheck());
}

TEST(BoardAttackTest, PawnsAttackDiagonallyWithoutWrappingFiles) {
    Board board;
    board.setFEN("7k/8/8/8/8/8/P7/4K3 w - - 0 1");

    EXPECT_TRUE(board.isSquareAttacked(B3, WHITE));
    EXPECT_FALSE(board.isSquareAttacked(A3, WHITE));
    EXPECT_FALSE(board.isSquareAttacked(H2, WHITE));

    board.setFEN("4k3/p7/8/8/8/8/8/7K b - - 0 1");

    EXPECT_TRUE(board.isSquareAttacked(B6, BLACK));
    EXPECT_FALSE(board.isSquareAttacked(A6, BLACK));
    EXPECT_FALSE(board.isSquareAttacked(H7, BLACK));
}

TEST(BoardAttackTest, KingAttacksOnlyAdjacentSquaresAtBoardEdge) {
    Board board;
    board.setFEN("k7/8/8/8/8/8/8/7K w - - 0 1");

    EXPECT_TRUE(board.isSquareAttacked(A7, BLACK));
    EXPECT_TRUE(board.isSquareAttacked(B7, BLACK));
    EXPECT_TRUE(board.isSquareAttacked(B8, BLACK));
    EXPECT_FALSE(board.isSquareAttacked(C7, BLACK));
    EXPECT_FALSE(board.isSquareAttacked(H7, BLACK));
}

TEST(BoardAttackTest, DetectsDistantSlidersAndStopsAtFirstBlocker) {
    Board board;
    board.setFEN("k3r3/8/8/8/8/8/8/K7 w - - 0 1");
    EXPECT_TRUE(board.isSquareAttacked(E1, BLACK));

    board.setFEN("k3r3/8/8/8/4P3/8/8/K7 w - - 0 1");
    EXPECT_FALSE(board.isSquareAttacked(E1, BLACK));

    board.setFEN("k6b/8/8/8/8/8/8/K7 w - - 0 1");
    EXPECT_TRUE(board.isSquareAttacked(A1, BLACK));

    board.setFEN("k2q4/8/8/8/8/8/8/K7 w - - 0 1");
    EXPECT_TRUE(board.isSquareAttacked(D1, BLACK));
}

TEST(BoardAttackTest, TreatsDefendingKingAsTransparentAsDocumented) {
    Board board;
    board.setFEN("k3r3/8/8/8/4K3/8/8/8 w - - 0 1");

    EXPECT_TRUE(board.isSquareAttacked(E1, BLACK));
}

TEST(BoardTest, EveryGeneratedStartingMoveCanBeUndoneExactly) {
    Board board;
    board.setStartingPosition();
    const auto initial = publicState(board);

    MoveList moves;
    board.generateMoves(moves);
    ASSERT_EQ(moves.size, 20);

    for (const Move move : moves) {
        SCOPED_TRACE(static_cast<unsigned>(move));
        EXPECT_EQ(moveType(move), NORMAL);
        EXPECT_NE(moveFrom(move), moveTo(move));
        EXPECT_NE(board.pieceAt(moveFrom(move)), NO_PIECE);
        EXPECT_EQ(board.pieceAt(moveTo(move)), NO_PIECE);
        board.makeMove(move);
        board.undoMove(move);
        expectSameState(initial, board);
    }
}

TEST(BoardTest, DoublePawnPushUpdatesTurnAndEnPassantState) {
    Board board;
    board.setStartingPosition();
    const auto initial = publicState(board);

    MoveList moves;
    board.generateMoves(moves);
    const Move e2e4 = encodeMove(E2, E4);
    bool foundE2E4 = false;
    for (const Move move : moves)
        foundE2E4 |= move == e2e4;

    ASSERT_TRUE(foundE2E4);
    board.makeMove(e2e4);
    EXPECT_EQ(board.pieceAt(E2), NO_PIECE);
    EXPECT_EQ(board.pieceAt(E4), W_PAWN);
    EXPECT_EQ(board.sideToMove(), BLACK);
    EXPECT_TRUE(board.hasEnPassant());
    EXPECT_EQ(board.enPassantFile(), FILE_E);
    EXPECT_EQ(board.enPassantSquare(), E3);
    EXPECT_EQ(board.halfmoveClock(), 0);
    EXPECT_EQ(board.fullmoveNumber(), 1);
    board.undoMove(e2e4);
    expectSameState(initial, board);
}

TEST(BoardTest, EnPassantThatExposesOwnKingIsNotGenerated) {
    Board board;
    // Moving f5xg6 e.p. would remove both rank-five blockers and expose the
    // white king on e5 to the rook on h5.
    board.setFEN("k7/8/8/4KPpr/8/8/8/8 w - g6 0 1");

    MoveList moves;
    board.generateMoves(moves);
    ASSERT_GT(moves.size, 0);
    const Move illegal = encodeMove(F5, G6, EN_PASSANT);
    for (const Move move : moves) {
        EXPECT_NE(move, illegal);
    }
}

TEST(BoardTest, CannotCastleThroughAnAttackedSquare) {
    Board board;
    // The bishop on c4 attacks f1, so white may castle queenside but not
    // kingside.
    board.setFEN("r3k2r/8/8/8/2b5/8/8/R3K2R w KQkq - 0 1");

    MoveList moves;
    board.generateMoves(moves);
    const Move kingside = encodeMove(E1, H1, CASTLING);
    const Move queenside = encodeMove(E1, A1, CASTLING);
    bool foundKingsideCastle = false;
    bool foundQueensideCastle = false;
    for (const Move move : moves) {
        foundKingsideCastle |= move == kingside;
        foundQueensideCastle |= move == queenside;
    }

    EXPECT_FALSE(foundKingsideCastle);
    ASSERT_TRUE(foundQueensideCastle);
    board.makeMove(queenside);
    EXPECT_EQ(board.pieceAt(C1), W_KING);
    EXPECT_EQ(board.pieceAt(D1), W_ROOK);
    board.undoMove(queenside);
}

TEST(BoardTest, GeneratesAllFourPromotionChoices) {
    Board board;
    board.setFEN("7k/P7/8/8/8/8/8/7K w - - 0 1");

    MoveList moves;
    board.generateMoves(moves);
    for (int piece = KNIGHT; piece <= QUEEN; ++piece) {
        const Move promotion = encodeMove(A7, A8, PROMOTION,
                                          static_cast<PieceType>(piece));
        bool found = false;
        for (const Move move : moves)
            found |= move == promotion;

        ASSERT_TRUE(found) << "promotion to " << piece;
        board.makeMove(promotion);
        EXPECT_EQ(board.pieceAt(A7), NO_PIECE);
        EXPECT_EQ(board.pieceAt(A8), static_cast<Piece>(piece));
        board.undoMove(promotion);
        EXPECT_EQ(board.pieceAt(A7), W_PAWN);
    }
}

TEST(BoardTest, HashTracksPositionAndIsRestoredByUndo) {
    Board board;
    board.setStartingPosition();
    const std::uint64_t startingHash = board.hash();

    MoveList moves;
    board.generateMoves(moves);
    ASSERT_GT(moves.size, 0);
    board.makeMove(moves[0]);
    EXPECT_NE(board.hash(), startingHash);
    board.undoMove(moves[0]);
    EXPECT_EQ(board.hash(), startingHash);
}
