#pragma once

#include "types.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <ranges>
#include <string_view>

constexpr int MAX_MOVES = 256;

// MAX IS 218 FOR LEGALLY REACHABLE POSITION BUT THEORETICALLY COULD BE 267

constexpr int MAX_PLY = 256;

constexpr std::string_view STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

using PackedState = std::uint8_t;

// bits 0 to 3 -> Castling {WK, WQ, BK, BQ}
// bit 7 -> en pass flag
// bit 4 to 6 -> en pass file

constexpr std::uint8_t CASTLING_MASK = 0x0F;
constexpr std::uint8_t EP_FILE_MASK  = 0x70;
constexpr std::uint8_t EP_FLAG       = 0x80;

enum CastlingRight : std::uint8_t {
    WHITE_KINGSIDE  = 1 << 0,
    WHITE_QUEENSIDE = 1 << 1,
    BLACK_KINGSIDE  = 1 << 2,
    BLACK_QUEENSIDE = 1 << 3
};

constexpr std::uint8_t castlingRights(PackedState state) {
    return state & CASTLING_MASK;
}

constexpr bool hasEnPassant(PackedState state) {
    return (state & EP_FLAG) != 0;
}

constexpr File enPassantFile(PackedState state) {
    return static_cast<File>((state & EP_FILE_MASK) >> 4);
}

constexpr PackedState makePackedState(
    std::uint8_t castling,
    bool hasEP,
    File epFile = FILE_A
) {
    return static_cast<PackedState>(
        (castling & CASTLING_MASK) |
        (
            hasEP
            ? EP_FLAG |
              ((static_cast<std::uint8_t>(epFile) & 0x07) << 4)
            : 0
        )
    );
}

struct UndoState {
    Piece capturedPiece = NO_PIECE;
    PackedState state = 0;
    std::uint16_t halfmoveClock = 0;
    std::uint64_t zobristKey = 0;
};

struct MoveList {
    std::array<Move, MAX_MOVES> moves{};
    int size = 0;

    void clear() {
        size = 0;
    }

    void add(Move move) {
        assert(size < MAX_MOVES);
        moves[size++] = move;
    }

    Move& operator[](int index) {
        return moves[index];
    }

    const Move& operator[](int index) const {
        return moves[index];
    }

    Move* begin() {
        return moves.data();
    }

    Move* end() {
        return moves.data() + size;
    }

    const Move* begin() const {
        return moves.data();
    }

    const Move* end() const {
        return moves.data() + size;
    }
};

class Board {

private:
    Bitboard pieces_[NUM_COLOUR][NUM_PIECE_TYPE]{};

    // USE https://chessprogramming.org/Sliding_Pieces

    Bitboard occupancy_[NUM_COLOUR]{};
    Bitboard occupancyAll_ = 0;

    std::array<Piece, NUM_SQUARE> board_{}; // mailbox

    // Cached for each position, as in Stockfish's king-blocker/check state.
    // A blocker may be either colour; pinned pieces are blockersForKing_[c] & occupancy_[c].
    Bitboard blockersForKing_[NUM_COLOUR]{};
    Bitboard pinners_[NUM_COLOUR]{};
    Bitboard checkers_ = 0; // attackers of sideToMove_'s king

    Colour sideToMove_ = WHITE;

    PackedState state_ = 0;

    std::uint16_t halfmoveClock_ = 0;
    std::uint16_t fullmoveNumber_ = 1;

    std::uint64_t zobristKey_ = 0;

    std::array<UndoState, MAX_PLY> history_{};
    int ply_ = 0;

    void addPiece(Piece piece, Square sq);
    void removePiece(Piece piece, Square sq);
    void movePiece(Piece piece, Square from, Square to);

    void updateOccupancy();
    void updateSliderBlockers(Colour colour);
    bool legalCandidate(Move move) const;

    void generatePawnMoves(MoveList& moves) const;
    void generateKnightMoves(MoveList& moves) const;
    void generateBishopMoves(MoveList& moves) const;
    void generateRookMoves(MoveList& moves) const;
    void generateQueenMoves(MoveList& moves) const;
    void generateKingMoves(MoveList& moves) const;

public:
    Board();

    void setStartingPosition();
    void setFEN(std::string_view fen);

    void setPins();

    void generateMoves(MoveList& moves) const;

    void makeMove(Move move);
    void undoMove(Move move);

    bool inCheck() const;
    bool isSquareAttacked(Square sq, Colour by) const;

    Piece pieceAt(Square sq) const {
        return board_[sq];
    }

    Bitboard pieces(Colour colour, PieceType type) const {
        return pieces_[colour][type];
    }

    Bitboard occupancy(Colour colour) const {
        return occupancy_[colour];
    }

    Bitboard occupancy() const {
        return occupancyAll_;
    }

    Colour sideToMove() const {
        return sideToMove_;
    }

    std::uint8_t castlingRights() const {
        return ::castlingRights(state_);
    }

    bool hasEnPassant() const {
        return ::hasEnPassant(state_);
    }

    File enPassantFile() const {
        return ::enPassantFile(state_);
    }

    Square enPassantSquare() const;

    std::uint16_t halfmoveClock() const {
        return halfmoveClock_;
    }

    std::uint16_t fullmoveNumber() const {
        return fullmoveNumber_;
    }

    std::uint64_t hash() const {
        return zobristKey_;
    }
};
