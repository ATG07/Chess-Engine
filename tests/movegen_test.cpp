#include "board.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

// Reflect ranks and swap colours so every focused position tests both sides.
std::string oppositePosition(std::string_view fen) {
    std::istringstream input{std::string(fen)};
    std::array<std::string, 6> fields;
    for (auto& field : fields)
        input >> field;

    std::istringstream placement(fields[0]);
    std::array<std::string, 8> ranks;
    for (auto& rank : ranks)
        std::getline(placement, rank, '/');

    auto swapColour = [](char piece) {
        if (piece >= 'A' && piece <= 'Z')
            return static_cast<char>(piece - 'A' + 'a');
        if (piece >= 'a' && piece <= 'z')
            return static_cast<char>(piece - 'a' + 'A');
        return piece;
    };

    std::string result;
    for (int rank = 7; rank >= 0; --rank) {
        if (!result.empty())
            result += '/';
        for (char piece : ranks[rank])
            result += swapColour(piece);
    }
    fields[1] = fields[1] == "w" ? "b" : "w";
    for (char& right : fields[2])
        right = swapColour(right);
    if (fields[3] != "-")
        fields[3][1] = static_cast<char>('1' + '8' - fields[3][1]);

    for (int field = 1; field < 6; ++field)
        result += " " + fields[field];
    return result;
}

class BoardMoveGenerationTest : public testing::TestWithParam<bool> {
protected:
    Board board;
    MoveList moves;

    void load(std::string_view fen) {
        const std::string position = GetParam() ? oppositePosition(fen) : std::string(fen);
        RecordProperty("fen", position);
        // Deliberately rely on setFEN to initialise the pin/check cache.
        board.setFEN(position);
        board.generateMoves(moves);
    }

    Square square(Square sq) const {
        return GetParam() ? static_cast<Square>(sq ^ 56) : sq;
    }

    void expectMoves(std::initializer_list<Move> expected, Square from = NO_SQUARE,
                     bool nonKingOnly = false) {
        std::vector<Move> actual;
        for (Move move : moves) {
            if (from != NO_SQUARE && moveFrom(move) != square(from))
                continue;
            if (nonKingOnly && getPieceType(board.pieceAt(moveFrom(move))) == KING)
                continue;
            actual.push_back(move);
        }

        std::vector<Move> wanted;
        for (Move move : expected) {
            wanted.push_back(encodeMove(square(moveFrom(move)), square(moveTo(move)),
                                        moveType(move), promotionType(move)));
        }
        std::sort(actual.begin(), actual.end());
        std::sort(wanted.begin(), wanted.end());
        EXPECT_EQ(actual, wanted); // Exact multiset: catches missing, extra and duplicate moves.
    }
};

TEST_P(BoardMoveGenerationTest, StartingPositionHasExactlyTwentyMoves) {
    load(STARTING_FEN);
    expectMoves({
        encodeMove(A2, A3), encodeMove(A2, A4),
        encodeMove(B2, B3), encodeMove(B2, B4),
        encodeMove(C2, C3), encodeMove(C2, C4),
        encodeMove(D2, D3), encodeMove(D2, D4),
        encodeMove(E2, E3), encodeMove(E2, E4),
        encodeMove(F2, F3), encodeMove(F2, F4),
        encodeMove(G2, G3), encodeMove(G2, G4),
        encodeMove(H2, H3), encodeMove(H2, H4),
        encodeMove(B1, A3), encodeMove(B1, C3),
        encodeMove(G1, F3), encodeMove(G1, H3),
    });
}

TEST_P(BoardMoveGenerationTest, PinnedKnightCannotMoveButOtherKnightCan) {
    load("k3r3/8/8/8/8/8/4N3/4K1N1 w - - 0 1");
    expectMoves({}, E2);
    expectMoves({encodeMove(G1, F3), encodeMove(G1, H3)}, G1);
}

TEST_P(BoardMoveGenerationTest, FilePinnedPawnCanPushButCannotCaptureOffLine) {
    load("k3r3/8/8/8/8/3p1p2/4P3/4K3 w - - 0 1");
    expectMoves({encodeMove(E2, E3), encodeMove(E2, E4)}, E2);
}

TEST_P(BoardMoveGenerationTest, RankPinnedPawnCannotPush) {
    load("k7/8/8/KP5r/8/8/8/8 w - - 0 1");
    expectMoves({}, B5);
}

TEST_P(BoardMoveGenerationTest, DiagonalPinnedPawnCanCaptureItsPinner) {
    load("k7/8/8/4b3/3P4/2K5/8/8 w - - 0 1");
    expectMoves({encodeMove(D4, E5)}, D4);
}

TEST_P(BoardMoveGenerationTest, PinnedRookCanSlideAlongFileAndCapturePinner) {
    load("k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    expectMoves({encodeMove(E2, E3), encodeMove(E2, E4), encodeMove(E2, E5),
                 encodeMove(E2, E6), encodeMove(E2, E7), encodeMove(E2, E8)}, E2);
}

TEST_P(BoardMoveGenerationTest, PinnedBishopCanSlideAlongDiagonalAndCapturePinner) {
    load("k7/8/7b/8/8/8/3B4/2K5 w - - 0 1");
    expectMoves({encodeMove(D2, E3), encodeMove(D2, F4),
                 encodeMove(D2, G5), encodeMove(D2, H6)}, D2);
}

TEST_P(BoardMoveGenerationTest, PinnedQueenCanMoveBothWaysAlongRank) {
    load("k7/8/8/8/K2Q3r/8/8/8 w - - 0 1");
    expectMoves({encodeMove(D4, B4), encodeMove(D4, C4), encodeMove(D4, E4),
                 encodeMove(D4, F4), encodeMove(D4, G4), encodeMove(D4, H4)}, D4);
}

TEST_P(BoardMoveGenerationTest, IncompatibleSliderCannotMoveAlongPin) {
    load("k7/8/7b/8/8/8/3R4/2K5 w - - 0 1");
    expectMoves({}, D2);

    load("k3r3/8/8/8/8/8/4B3/4K3 w - - 0 1");
    expectMoves({}, E2);
}

TEST_P(BoardMoveGenerationTest, TwoBlockersDoNotRestrictRookToPinLine) {
    load("k3r3/8/8/8/8/4P3/4R3/4K3 w - - 0 1");
    expectMoves({encodeMove(E2, A2), encodeMove(E2, B2), encodeMove(E2, C2),
                 encodeMove(E2, D2), encodeMove(E2, F2), encodeMove(E2, G2),
                 encodeMove(E2, H2)}, E2);
}

TEST_P(BoardMoveGenerationTest, BishopCanBlockRookCheckButCannotIgnoreIt) {
    load("k3r3/8/8/8/8/2B5/8/4K3 w - - 0 1");
    expectMoves({encodeMove(C3, E5)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, KnightCanInterposeOnCheckingFile) {
    load("k3r3/8/8/8/8/8/2N5/4K3 w - - 0 1");
    expectMoves({encodeMove(C2, E3)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, KnightCheckCanOnlyBeCapturedByNonKing) {
    load("k7/8/8/8/8/5n2/8/4KR2 w - - 0 1");
    expectMoves({encodeMove(F1, F3)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, PawnCheckCanOnlyBeCapturedByNonKing) {
    load("k7/8/8/8/8/8/3p4/3RK3 w - - 0 1");
    expectMoves({encodeMove(D1, D2)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, PawnCanBlockDiagonalCheck) {
    load("k7/8/8/8/1b6/8/2P5/4K3 w - - 0 1");
    expectMoves({encodeMove(C2, C3)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, DoublePushCanBlockEvenWhenSinglePushCannot) {
    load("k7/8/1b6/8/8/8/3P1K2/8 w - - 0 1");
    expectMoves({encodeMove(D2, D4)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, DoubleCheckAllowsOnlyExactKingEscapes) {
    load("k3r3/8/8/8/1b6/8/8/R3K3 w - - 0 1");
    expectMoves({encodeMove(E1, D1), encodeMove(E1, F1), encodeMove(E1, F2)});
}

TEST_P(BoardMoveGenerationTest, PinnedPieceCannotCaptureDifferentChecker) {
    // Be2xf3 captures the checking knight but would expose the e8 rook.
    load("k3r3/8/8/8/8/5n2/4B3/4K3 w - - 0 1");
    expectMoves({}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, EnPassantCanCaptureCheckingPawnOffDestination) {
    load("k7/8/8/3pP3/4K3/8/8/8 w - d6 0 1");
    expectMoves({encodeMove(E5, D6, EN_PASSANT)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, EnPassantCanInterposeOnDiagonalCheck) {
    load("kb6/8/8/2Pp4/8/8/7K/8 w - d6 0 1");
    expectMoves({encodeMove(C5, D6, EN_PASSANT)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, EnPassantCannotExposeHorizontalRookAttack) {
    load("k7/8/8/4KPpr/8/8/8/8 w - g6 0 1");
    expectMoves({encodeMove(F5, F6)}, F5);
}

TEST_P(BoardMoveGenerationTest, EnPassantCannotExposeDiagonalBishopAttack) {
    load("k7/8/5b2/3Pp3/8/2K5/8/8 w - e6 0 1");
    expectMoves({encodeMove(D5, D6)}, D5);
}

TEST_P(BoardMoveGenerationTest, DiagonalPinnedPawnCanEnPassantAlongPinLine) {
    load("k7/5b2/8/3Pp3/2K5/8/8/8 w - e6 0 1");
    expectMoves({encodeMove(D5, E6, EN_PASSANT)}, D5);
}

TEST_P(BoardMoveGenerationTest, EnPassantCannotIgnoreUnrelatedKnightCheck) {
    load("k7/8/8/3pP3/8/5n2/8/4K3 w - d6 0 1");
    expectMoves({}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, OnlyUnpinnedPawnCanUseSharedEnPassantSquare) {
    load("k1r5/8/8/2PpP3/8/8/8/2K5 w - d6 0 1");
    expectMoves({encodeMove(C5, C6)}, C5);
    expectMoves({encodeMove(E5, E6), encodeMove(E5, D6, EN_PASSANT)}, E5);
}

TEST_P(BoardMoveGenerationTest, PawnCannotDoublePushThroughOccupiedSquare) {
    load("k7/8/8/8/8/4N3/4P3/4K3 w - - 0 1");
    expectMoves({}, E2);
}

TEST_P(BoardMoveGenerationTest, KingCannotCapturePieceDefendedBySliderBehindIt) {
    load("k3r3/8/8/8/8/8/4n3/4K3 w - - 0 1");
    expectMoves({encodeMove(E1, D1), encodeMove(E1, F1),
                 encodeMove(E1, D2), encodeMove(E1, F2)});
}

TEST_P(BoardMoveGenerationTest, KingCannotRetreatDownLineThroughItsOldSquare) {
    load("k3r3/8/8/8/8/8/4K3/8 w - - 0 1");
    expectMoves({encodeMove(E2, D1), encodeMove(E2, F1), encodeMove(E2, D2),
                 encodeMove(E2, F2), encodeMove(E2, D3), encodeMove(E2, F3)});
}

TEST_P(BoardMoveGenerationTest, KingAtCornerDoesNotWrapOrCaptureFriendlyPawn) {
    load("k7/8/8/8/8/8/7P/7K w - - 0 1");
    expectMoves({encodeMove(H1, G1), encodeMove(H1, G2)}, H1);
}

TEST_P(BoardMoveGenerationTest, PinnedEnemyKnightStillAttacksKingDestinations) {
    // Nd3 is pinned to the black king by Rd1, but still attacks f2 and f4.
    load("3k4/8/8/8/8/3n4/4K3/3R4 w - - 0 1");
    expectMoves({encodeMove(E2, F1), encodeMove(E2, D2), encodeMove(E2, D3),
                 encodeMove(E2, E3), encodeMove(E2, F3)}, E2);
}

TEST_P(BoardMoveGenerationTest, CastlingCanCrossSafeSquaresWhenBFileIsAttacked) {
    load("1r5k/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    expectMoves({encodeMove(E1, D1), encodeMove(E1, F1), encodeMove(E1, D2),
                 encodeMove(E1, E2), encodeMove(E1, F2),
                 encodeMove(E1, A1, CASTLING), encodeMove(E1, H1, CASTLING)}, E1);
}

TEST_P(BoardMoveGenerationTest, CastlingCannotEscapeCheck) {
    load("k3r3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    expectMoves({encodeMove(E1, D1), encodeMove(E1, F1),
                 encodeMove(E1, D2), encodeMove(E1, F2)}, E1);
}

TEST_P(BoardMoveGenerationTest, PromotionCaptureOfCheckerGeneratesAllFourChoices) {
    load("k6r/6P1/8/8/8/8/8/7K w - - 0 1");
    expectMoves({encodeMove(G7, H8, PROMOTION, KNIGHT),
                 encodeMove(G7, H8, PROMOTION, BISHOP),
                 encodeMove(G7, H8, PROMOTION, ROOK),
                 encodeMove(G7, H8, PROMOTION, QUEEN)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, PromotionCanBlockCheckOrCaptureChecker) {
    load("K6r/6P1/8/8/8/8/8/k7 w - - 0 1");
    expectMoves({encodeMove(G7, G8, PROMOTION, KNIGHT),
                 encodeMove(G7, G8, PROMOTION, BISHOP),
                 encodeMove(G7, G8, PROMOTION, ROOK),
                 encodeMove(G7, G8, PROMOTION, QUEEN),
                 encodeMove(G7, H8, PROMOTION, KNIGHT),
                 encodeMove(G7, H8, PROMOTION, BISHOP),
                 encodeMove(G7, H8, PROMOTION, ROOK),
                 encodeMove(G7, H8, PROMOTION, QUEEN)}, NO_SQUARE, true);
}

TEST_P(BoardMoveGenerationTest, PinnedPromotionMustCaptureAlongDiagonal) {
    load("k6b/6P1/5K2/8/8/8/8/8 w - - 0 1");
    expectMoves({encodeMove(G7, H8, PROMOTION, KNIGHT),
                 encodeMove(G7, H8, PROMOTION, BISHOP),
                 encodeMove(G7, H8, PROMOTION, ROOK),
                 encodeMove(G7, H8, PROMOTION, QUEEN)}, G7);
}

TEST_P(BoardMoveGenerationTest, CheckmateAndStalemateHaveNoMoves) {
    load("8/8/8/8/8/5k2/6q1/7K w - - 0 1");
    ASSERT_NE(board.checkers(), 0);
    expectMoves({});

    load("8/8/8/8/8/6q1/5k2/7K w - - 0 1");
    ASSERT_EQ(board.checkers(), 0);
    expectMoves({});
}

INSTANTIATE_TEST_SUITE_P(BothColours, BoardMoveGenerationTest, testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
        return info.param ? "Black" : "White";
    });

// Root counts exercise the public generator without unfinished makeMove/undoMove.
TEST(BoardMoveGenerationRootTest, MatchesStandardPerftPositionsAtDepthOne) {
    struct Position {
        std::string_view name;
        std::string_view fen;
        int moves;
    };
    constexpr Position positions[] = {
        {"Start", STARTING_FEN, 20},
        {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 48},
        {"Endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 14},
        {"Asymmetric", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6},
        {"Promotion", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 44},
        {"Middlegame", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 46},
    };

    for (const auto& position : positions) {
        SCOPED_TRACE(position.name);
        Board board;
        board.setFEN(position.fen);
        MoveList moves;
        moves.add(encodeMove(A1, A2)); // generateMoves must replace an existing list.
        board.generateMoves(moves);
        EXPECT_EQ(moves.size, position.moves);

        std::vector<Move> unique(moves.begin(), moves.end());
        std::sort(unique.begin(), unique.end());
        EXPECT_EQ(std::adjacent_find(unique.begin(), unique.end()), unique.end());
    }
}

} // namespace
