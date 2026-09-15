#include "board.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

namespace {

bool containsMove(const MoveList& moves, Move expected) {
    for (const Move move : moves)
        if (move == expected)
            return true;
    return false;
}

std::uint64_t perft(Board& board, int depth) {
    if (depth == 0) {
        return 1;
    }

    MoveList moves;
    board.generateMoves(moves);

    if (depth == 1) {
        return static_cast<std::uint64_t>(moves.size);
    }

    std::uint64_t nodes = 0;
    for (const Move move : moves) {
        board.makeMove(move);
        nodes += perft(board, depth - 1);
        board.undoMove(move);
    }
    return nodes;
}

void expectPerft(std::string_view fen, int depth, std::uint64_t nodes) {
    Board board;
    board.setFEN(fen);
    EXPECT_EQ(perft(board, depth), nodes) << "at depth " << depth;
}

}  // namespace

TEST(PerftTest, RootSpecialMovesUseStockfishEncoding) {
    Board board;
    MoveList moves;

    board.setFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    board.generateMoves(moves);
    EXPECT_TRUE(containsMove(moves, encodeMove(E1, H1, CASTLING)));
    EXPECT_TRUE(containsMove(moves, encodeMove(E1, A1, CASTLING)));

    moves.clear();
    board.setFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    board.generateMoves(moves);
    EXPECT_TRUE(containsMove(moves, encodeMove(E5, D6, EN_PASSANT)));

    moves.clear();
    board.setFEN("4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1");
    board.generateMoves(moves);
    EXPECT_TRUE(containsMove(moves, encodeMove(E4, D3, EN_PASSANT)));

    moves.clear();
    board.setFEN("7k/P7/8/8/8/8/8/7K w - - 0 1");
    board.generateMoves(moves);
    for (int piece = KNIGHT; piece <= QUEEN; ++piece)
        EXPECT_TRUE(containsMove(moves, encodeMove(
            A7, A8, PROMOTION, static_cast<PieceType>(piece))));
}

TEST(PerftTest, StartingPosition) {
    constexpr std::string_view start =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    expectPerft(start, 0, 1);
    expectPerft(start, 1, 20);
    expectPerft(start, 2, 400);
    expectPerft(start, 3, 8'902);
    expectPerft(start, 4, 197'281);
}

TEST(PerftTest, KiwipeteExercisesCastlingAndPins) {
    constexpr std::string_view kiwipete =
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R "
        "w KQkq - 0 1";

    expectPerft(kiwipete, 1, 48);
    expectPerft(kiwipete, 2, 2'039);
    expectPerft(kiwipete, 3, 97'862);
}

TEST(PerftTest, EndgamePositionExercisesEnPassantAndChecks) {
    constexpr std::string_view position =
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";

    expectPerft(position, 1, 14);
    expectPerft(position, 2, 191);
    expectPerft(position, 3, 2'812);
    expectPerft(position, 4, 43'238);
}

TEST(PerftTest, AsymmetricCastlingPositionExercisesPromotionsAndChecks) {
    constexpr std::string_view position =
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 "
        "w kq - 0 1";

    expectPerft(position, 1, 6);
    expectPerft(position, 2, 264);
    expectPerft(position, 3, 9'467);
    expectPerft(position, 4, 422'333);
}

TEST(PerftTest, PromotionPositionExercisesDiscoveredChecks) {
    constexpr std::string_view position =
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R "
        "w KQ - 1 8";

    expectPerft(position, 1, 44);
    expectPerft(position, 2, 1'486);
    expectPerft(position, 3, 62'379);
    expectPerft(position, 4, 2'103'487);
}

TEST(PerftTest, TacticalMiddlegameExercisesPinsAndCheckEvasions) {
    constexpr std::string_view position =
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 "
        "w - - 0 10";

    expectPerft(position, 1, 46);
    expectPerft(position, 2, 2'079);
    expectPerft(position, 3, 89'890);
    expectPerft(position, 4, 3'894'594);
}
