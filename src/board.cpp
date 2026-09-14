#include <ranges>
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include "board.hpp"

Board::Board(){
    // Should be enough for now ig?
    board_.fill(NO_PIECE);
    state_ = 0;
}

void Board::addPiece(Piece piece, Square square) {
    board_[square] = piece;

    Colour colour = getColour(piece);
    PieceType pieceType = getPieceType(piece);

    pieces_[colour][pieceType]  |= squareBit(square);
    occupancy_[colour]          |= squareBit(square);
    occupancyAll_               |= squareBit(square);
}

void Board::removePiece(Piece piece, Square square) {
    // if (board_[square] != piece){std::__throw_runtime_error("Cannot remove piece as does not exist");}
    // SHOULD SPECIFY PIECE OR REMOVE LINE OF CODE IF REDUNDANT

    board_[square] = NO_PIECE;

    Colour colour = getColour(piece);
    PieceType pieceType = getPieceType(piece);
    
    pieces_[colour][pieceType]  &= ~squareBit(square);
    occupancy_[colour]          &= ~squareBit(square);
    occupancyAll_               &= ~squareBit(square);
}

void Board::movePiece(Piece piece, Square from, Square to) {
    board_[from] = NO_PIECE;
    board_[to] = piece;

    Colour colour = getColour(piece);
    PieceType pieceType = getPieceType(piece);
    
    Bitboard mask = (squareBit(from) ^ squareBit(to));

    pieces_[colour][pieceType]  ^= mask;
    occupancy_[colour]          ^= mask;
    occupancyAll_               ^= mask;
}

void Board::updateOccupancy() {
    occupancyAll_ = 0;
    occupancy_[WHITE] = 0;
    occupancy_[BLACK] = 0;

    for (int c = WHITE; c < NUM_COLOUR; c++){
        Colour colour = static_cast<Colour>(c);

        for (int p = 0; p < NUM_PIECE_TYPE; p++){
            PieceType pieceType = static_cast<PieceType>(p);

            occupancy_[colour] |= pieces_[colour][pieceType];
        }
    }

    occupancyAll_ = occupancy_[WHITE] | occupancy_[BLACK];
}

void Board::generatePawnMoves(MoveList&) const {}

void Board::generateKnightMoves(MoveList&) const {}

void Board::generateBishopMoves(MoveList&) const {}

void Board::generateRookMoves(MoveList&) const {}

void Board::generateQueenMoves(MoveList&) const {}

void Board::generateKingMoves(MoveList&) const {}

void Board::setStartingPosition() {
    setFEN(STARTING_FEN);
}

void Board::setFEN(std::string_view fen) {
    for (int c = WHITE; c < NUM_COLOUR; c++){
        Colour colour = static_cast<Colour>(c);

        for (int p = 0; p < NUM_PIECE_TYPE; p++){
            PieceType pieceType = static_cast<PieceType>(p);

            pieces_[colour][pieceType] = 0;
        }

        occupancy_[colour] = 0;
    }

    board_.fill(NO_PIECE);
    occupancyAll_ = 0;
    state_ = 0;
    halfmoveClock_ = 0;
    fullmoveNumber_ = 1;
    zobristKey_ = 0;
    ply_ = 0;

    auto fenFields = fen 
                | std::views::split(' ') 
                | std::views::transform([](auto&& field) {
                      return std::string_view(field.begin(), field.end());
                  });

    if (std::ranges::distance(fenFields) != 6)
        throw std::invalid_argument("FEN must have 6 fields");

    std::array<std::string_view, 6> fields{};
    std::ranges::copy(fenFields, fields.begin());

    auto boardField = fields[0];

    auto ranks = boardField
                | std::views::split('/') 
                | std::views::transform([](auto&& row) {
                      return std::string_view(row.begin(), row.end());
                  });

    if (std::ranges::distance(ranks) != 8)
        throw std::invalid_argument("FEN must have 8 ranks");

    std::array<std::string_view, 8> board_rank{};
    std::ranges::copy(ranks, board_rank.begin());


    constexpr std::string_view pieceCharacters = "PNBRQKpnbrqk";

    for (int rank = 0; rank < 8; ++rank) {
        int file = 0;

        for (char character : board_rank[rank]) {
            if (character >= '1' && character <= '8') {
                file += character - '0';
                continue;
            }

            const auto piece = pieceCharacters.find(character);
            if (piece == std::string_view::npos || file >= 8)
                throw std::invalid_argument("Invalid FEN board");

            addPiece(static_cast<Piece>(piece),
                     makeSquare(static_cast<File>(file++),
                                static_cast<Rank>(7 - rank)));
        }

        if (file != 8){
            throw std::invalid_argument("Invalid FEN rank");
        }
    }

    if (fields[1] == "w")
        sideToMove_ = WHITE;
    else if (fields[1] == "b")
        sideToMove_ = BLACK;
    else
        throw std::invalid_argument("Invalid side to move");

    std::uint8_t castling = 0;

    if (fields[2] != "-") {
        for (char right : fields[2]) {
            switch (right) {
                case 'K': castling |= WHITE_KINGSIDE;  break;
                case 'Q': castling |= WHITE_QUEENSIDE; break;
                case 'k': castling |= BLACK_KINGSIDE;  break;
                case 'q': castling |= BLACK_QUEENSIDE; break;
                default: throw std::invalid_argument("Invalid castling rights");
            }
        }
    }

    bool hasEP = fields[3] != "-";
    File epFile = FILE_A;

    if (hasEP) {
        char expectedRank = sideToMove_ == WHITE ? '6' : '3';

        if (fields[3].size() != 2 ||
            fields[3][0] < 'a' || fields[3][0] > 'h' ||
            fields[3][1] != expectedRank)
            throw std::invalid_argument("Invalid en passant square");

        epFile = static_cast<File>(fields[3][0] - 'a');
    }

    state_ = makePackedState(castling, hasEP, epFile);

    auto parseNumber = [](std::string_view field, auto& number) {
        auto [end, error] = std::from_chars(field.begin(), field.end(), number);

        if (error != std::errc{} || end != field.end())
            throw std::invalid_argument("Invalid FEN number");
    };

    parseNumber(fields[4], halfmoveClock_);
    parseNumber(fields[5], fullmoveNumber_);

    if (fullmoveNumber_ == 0)
        throw std::invalid_argument("Fullmove number must be positive");
}

void Board::generateMoves(MoveList&) const {}

void Board::makeMove(Move) {}

void Board::undoMove(Move) {}

bool Board::inCheck() const {
    return false;
}

bool Board::isSquareAttacked(Square, Colour) const {
    return false;
}

Square Board::enPassantSquare() const {
    if (!hasEnPassant())
        return NO_SQUARE;

    Rank rank = sideToMove_ == WHITE ? RANK_6 : RANK_3;
    return makeSquare(enPassantFile(), rank);
}
