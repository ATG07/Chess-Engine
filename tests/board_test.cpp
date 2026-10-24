#include "board.hpp"
#include "zobrist.hpp"

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

void expectPins(
    const Board& board,
    Colour kingColour,
    Bitboard pinned = 0,
    Bitboard pinners = 0) {
    EXPECT_EQ(board.blockersForKing(kingColour), pinned);
    EXPECT_EQ(board.pinnedToKing(kingColour), pinned);
    EXPECT_EQ(board.pinners(~kingColour), pinners);
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

TEST(BoardHashTest, FenHashDistinguishesSideToMove) {
    Board white, black;
    white.setFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    black.setFEN("4k3/8/8/8/8/8/8/4K3 b - - 0 1");

    EXPECT_EQ(white.hash() ^ black.hash(), Zobrist::side);
}

TEST(BoardHashTest, FenHashDistinguishesCastlingRights) {
    Board both, kingside, neither;
    both.setFEN("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    kingside.setFEN("4k3/8/8/8/8/8/8/R3K2R w K - 0 1");
    neither.setFEN("4k3/8/8/8/8/8/8/R3K2R w - - 0 1");

    EXPECT_EQ(both.hash() ^ neither.hash(),
              Zobrist::castling[WHITE_KINGSIDE | WHITE_QUEENSIDE] ^
                  Zobrist::castling[0]);
    EXPECT_EQ(kingside.hash() ^ neither.hash(),
              Zobrist::castling[WHITE_KINGSIDE] ^ Zobrist::castling[0]);
}

TEST(BoardHashTest, FenHashDistinguishesEnPassantFilesAndAbsence) {
    Board none, fileD, fileF;
    none.setFEN("4k3/8/8/3pPp2/8/8/8/4K3 w - - 0 1");
    fileD.setFEN("4k3/8/8/3pPp2/8/8/8/4K3 w - d6 0 1");
    fileF.setFEN("4k3/8/8/3pPp2/8/8/8/4K3 w - f6 0 1");

    EXPECT_EQ(none.hash() ^ fileD.hash(), Zobrist::enpassant[FILE_D]);
    EXPECT_EQ(none.hash() ^ fileF.hash(), Zobrist::enpassant[FILE_F]);
}

TEST(BoardHashTest, ReloadingFenReplacesHashAndIgnoresMoveCounters) {
    Board reused, fresh;
    reused.setStartingPosition();
    reused.setFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 42");
    fresh.setFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 19 90");

    EXPECT_EQ(reused.hash(), fresh.hash());
}

TEST(BoardHashTest, IncrementalMoveHashMatchesResultingFenWithoutUndo) {
    struct Transition {
        std::string_view before;
        Move move;
        std::string_view after;
    };
    const Transition transitions[] = {
        {STARTING_FEN, encodeMove(E2, E4),
         "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"},
        {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
         encodeMove(D7, D5),
         "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2"},
        {"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", encodeMove(E1, D1),
         "4k3/8/8/3pP3/8/8/8/3K4 b - - 1 1"},
        {"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", encodeMove(E5, D6, EN_PASSANT),
         "4k3/8/3P4/8/8/8/8/4K3 b - - 0 1"},
        {"4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1", encodeMove(E4, D3, EN_PASSANT),
         "4k3/8/8/8/8/3p4/8/4K3 w - - 0 2"},
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 8 1", encodeMove(E1, H1, CASTLING),
         "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 9 1"},
        {"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 8 1", encodeMove(E8, A8, CASTLING),
         "2kr3r/8/8/8/8/8/8/R3K2R w KQ - 9 2"},
        {"r3k2r/6B1/8/8/8/8/8/R3K2R w KQkq - 5 9", encodeMove(G7, H8),
         "r3k2B/8/8/8/8/8/8/R3K2R b KQq - 0 9"},
        {"4k2r/6P1/8/8/8/8/8/4K3 w k - 12 8", encodeMove(G7, H8, PROMOTION, QUEEN),
         "4k2Q/8/8/8/8/8/8/4K3 b - - 0 8"},
        {"7k/8/8/8/8/8/p7/7K b - - 4 7", encodeMove(A2, A1, PROMOTION, KNIGHT),
         "7k/8/8/8/8/8/8/n6K w - - 0 8"},
    };

    for (const auto& transition : transitions) {
        SCOPED_TRACE(transition.before);
        Board actual, expected;
        actual.setFEN(transition.before);
        expected.setFEN(transition.after);
        actual.makeMove(transition.move);

        EXPECT_EQ(actual.hash(), expected.hash());
    }
}

TEST(BoardTest, AttackDetectionHandlesLeapersAndSliders) {
    Board board;
    board.setFEN("4k3/8/2n5/3p4/8/8/4r3/4K3 w - - 0 1");

    EXPECT_TRUE(board.isSquareAttacked(E1, BLACK));  // rook
    EXPECT_TRUE(board.isSquareAttacked(E4, BLACK));  // pawn from d5
    EXPECT_TRUE(board.isSquareAttacked(E5, BLACK));  // knight from c6
    EXPECT_TRUE(board.isSquareAttacked(D4, BLACK));  // knight from c6
    EXPECT_FALSE(board.isSquareAttacked(A4, BLACK));
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

TEST(BoardPinsAttackersTest, StartingPositionHasNoPinsOrCheckers) {
    Board board;
    board.setStartingPosition();
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), 0);
    expectPins(board, WHITE);
}

TEST(BoardPinsAttackersTest, FindsPinsOnAllFourSliderLines) {
    Board board;
    board.setFEN(
        "k2r3b/b7/8/2RBN3/3KN2r/8/8/8 w - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), 0);
    expectPins(board, WHITE,
               squareBit(E4) | squareBit(D5) |
                   squareBit(C5) | squareBit(E5),
               squareBit(H4) | squareBit(D8) |
                   squareBit(A7) | squareBit(H8));
}

TEST(BoardPinsAttackersTest, IdentifiesEveryDirectCheckerType) {
    struct Case {
        std::string_view fen;
        Square checker;
    };

    constexpr Case cases[] = {
        {"k3r3/8/8/8/8/8/8/4K3 w - - 0 1", E8},
        {"k7/8/8/8/1b6/8/8/4K3 w - - 0 1", B4},
        {"k7/8/8/8/8/8/2n5/4K3 w - - 0 1", C2},
        {"k7/8/8/8/8/8/3p4/4K3 w - - 0 1", D2},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(test.fen);
        Board board;
        board.setFEN(test.fen);
        board.setPinsAttackers();

        EXPECT_EQ(board.checkers(), squareBit(test.checker));
        expectPins(board, WHITE);
    }
}

TEST(BoardPinsAttackersTest, RecordsBothPiecesInDoubleCheck) {
    Board board;
    board.setFEN("k3r3/8/8/8/1b6/8/8/4K3 w - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), squareBit(E8) | squareBit(B4));
    expectPins(board, WHITE);
}

TEST(BoardPinsAttackersTest, TwoPawnsCanCheckTheSameKing) {
    Board board;
    board.setFEN("k7/8/8/3p1p2/4K3/8/8/8 w - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), squareBit(D5) | squareBit(F5));
    expectPins(board, WHITE);
}

TEST(BoardPinsAttackersTest, TwoBlockersAndWrongSliderDoNotCreatePins) {
    Board board;
    board.setFEN("k3r3/8/8/8/8/4B3/4N3/4K3 w - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), 0);
    expectPins(board, WHITE);

    board.setFEN("k3r3/8/8/8/8/8/4n3/4K3 w - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), 0);
    EXPECT_EQ(board.blockersForKing(WHITE), squareBit(E2));
    EXPECT_EQ(board.pinnedToKing(WHITE), 0);
    EXPECT_EQ(board.pinners(BLACK), 0);

    board.setFEN("k3b3/8/8/8/8/8/4N3/4K3 w - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), 0);
    expectPins(board, WHITE);
}

TEST(BoardPinsAttackersTest, HandlesEdgeRayAndBlackSidePins) {
    Board board;
    board.setFEN("7b/8/8/8/8/8/1N6/K6k w - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), 0);
    expectPins(board, WHITE, squareBit(B2), squareBit(H8));

    board.setFEN("4k3/4n3/8/8/8/8/8/K3R3 b - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), 0);
    expectPins(board, BLACK, squareBit(E7), squareBit(E1));
}

TEST(BoardPinsAttackersTest, RecalculationClearsStalePinsAndCheckers) {
    Board board;
    board.setFEN("k3r3/8/8/8/8/8/4N3/4K3 w - - 0 1");
    board.setPinsAttackers();
    ASSERT_EQ(board.pinnedToKing(WHITE), squareBit(E2));

    board.setFEN("k7/8/8/8/8/8/8/4K3 w - - 0 1");
    board.setPinsAttackers();

    EXPECT_EQ(board.checkers(), 0);
    expectPins(board, WHITE);
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
        board.undoMove();
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
    board.undoMove();
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
    board.undoMove();
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
        board.undoMove();
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
    board.undoMove();
    EXPECT_EQ(board.hash(), startingHash);
}
