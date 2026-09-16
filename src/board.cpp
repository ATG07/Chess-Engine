#include <ranges>
#include <algorithm>
#include <bit>
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

void Board::generatePawnMoves(MoveList& moves) const {
    constexpr Bitboard BB_NOT_FINAL_RANK = ~BB_RANK_8 & ~BB_RANK_1;
    Bitboard single_move = (sideToMove_ == WHITE) ?
    ((pieces_[sideToMove_][PAWN]&BB_NOT_FINAL_RANK) << NUM_FILE) & ~occupancyAll_:
    ((pieces_[sideToMove_][PAWN]&BB_NOT_FINAL_RANK) >> NUM_FILE) & ~occupancyAll_;
    
    constexpr Bitboard BB_RANK_3 = BB_RANK_1 << (2 * NUM_FILE); 
    constexpr Bitboard BB_RANK_6 = BB_RANK_8 >> (2 * NUM_FILE); 

    Bitboard double_move = (sideToMove_ == WHITE) ?
    ((single_move&BB_RANK_3) << NUM_FILE) & ~occupancyAll_:
    ((single_move&BB_RANK_6) >> NUM_FILE) & ~occupancyAll_;

    Bitboard left_capture = (sideToMove_ == WHITE) ?
    ((pieces_[sideToMove_][PAWN]&(~BB_FILE_A)) << (NUM_FILE - 1)) & occupancy_[~sideToMove_]:
    ((pieces_[sideToMove_][PAWN]&(~BB_FILE_A)) >> (NUM_FILE + 1)) & occupancy_[~sideToMove_];

    Bitboard right_capture = (sideToMove_ == WHITE) ?
    ((pieces_[sideToMove_][PAWN]&(~BB_FILE_H)) << (NUM_FILE + 1)) & occupancy_[~sideToMove_]:
    ((pieces_[sideToMove_][PAWN]&(~BB_FILE_H)) >> (NUM_FILE - 1)) & occupancy_[~sideToMove_];

    const int forward = sideToMove_ == WHITE ? 8 : -8;
    constexpr Bitboard promotionRank = (BB_RANK_8 | BB_RANK_1);

    auto emit = [&]<MoveType type>(Bitboard destinations, int offset) {
        while (destinations) {
            const Square to = static_cast<Square>(std::countr_zero(destinations));
            const Square from = static_cast<Square>(static_cast<int>(to) - offset);
            destinations &= destinations - 1;

            if constexpr (type == PROMOTION) {
                for (int piece = KNIGHT; piece <= QUEEN; ++piece)
                    moves.add(encodeMove(from, to, PROMOTION,
                                         static_cast<PieceType>(piece)));
            } else {
                moves.add(encodeMove(from, to, type));
            }
        }
    };

    auto emitPawns = [&](Bitboard destinations, int offset) {
        emit.operator()<NORMAL>(destinations & ~promotionRank, offset);
        emit.operator()<PROMOTION>(destinations & promotionRank, offset);
    };

    emitPawns(single_move, forward);
    emit.operator()<NORMAL>(double_move, 2 * forward);
    emitPawns(left_capture, sideToMove_ == WHITE ? 7 : -9);
    emitPawns(right_capture, sideToMove_ == WHITE ? 9 : -7);

    if (hasEnPassant()) {
        const Square to = enPassantSquare();
        const Bitboard pawns = pieces_[sideToMove_][PAWN];

        const int leftFrom = static_cast<int>(to) -
                             (sideToMove_ == WHITE ? 7 : -9);
        const int rightFrom = static_cast<int>(to) -
                              (sideToMove_ == WHITE ? 9 : -7);

        if (fileof(to) != FILE_H &&
            (pawns & squareBit(static_cast<Square>(leftFrom))))
            moves.add(encodeMove(static_cast<Square>(leftFrom), to, EN_PASSANT));
        if (fileof(to) != FILE_A &&
            (pawns & squareBit(static_cast<Square>(rightFrom))))
            moves.add(encodeMove(static_cast<Square>(rightFrom), to, EN_PASSANT));
    }
}

void Board::generateKnightMoves(MoveList& moves) const {
    constexpr Bitboard notA = ~BB_FILE_A;
    constexpr Bitboard notH = ~BB_FILE_H;
    constexpr Bitboard notAB = ~(BB_FILE_A | (BB_FILE_A << 1));
    constexpr Bitboard notGH = ~(BB_FILE_H | (BB_FILE_H >> 1));
    const Bitboard knights = pieces_[sideToMove_][KNIGHT];

    auto emit = [&](Bitboard destinations, int offset) {
        destinations &= ~occupancy_[sideToMove_];
        while (destinations) {
            const Square to = static_cast<Square>(std::countr_zero(destinations));
            const Square from = static_cast<Square>(static_cast<int>(to) - offset);
            moves.add(encodeMove(from, to));
            destinations &= destinations - 1;
        }
    };

    emit((knights & notH) << 17, 17);
    emit((knights & notA) << 15, 15);
    emit((knights & notGH) << 10, 10);
    emit((knights & notAB) << 6, 6);
    emit((knights & notA) >> 17, -17);
    emit((knights & notH) >> 15, -15);
    emit((knights & notAB) >> 10, -10);
    emit((knights & notGH) >> 6, -6);
}

void Board::generateBishopMoves(MoveList& moves) const {
    Bitboard bishops = pieces_[sideToMove_][BISHOP];
    constexpr int delta[4] = {9, 7, -7, -9};

    while (bishops) {
        const Square from = static_cast<Square>(std::countr_zero(bishops));
        bishops &= bishops - 1;
        const int file = fileof(from);
        const int rank = rankof(from);
        const int limit[4] = {
            std::min(7 - file, 7 - rank), std::min(file, 7 - rank),
            std::min(7 - file, rank), std::min(file, rank)
        };

        for (int direction = 0; direction < 4; ++direction) {
            for (int step = 1; step <= limit[direction]; ++step) {
                const Square to = static_cast<Square>(
                    static_cast<int>(from) + step * delta[direction]);
                const Bitboard bit = squareBit(to);

                if (occupancy_[sideToMove_] & bit)
                    break;

                moves.add(encodeMove(from, to));
                if (occupancyAll_ & bit)
                    break;
            }
        }
    }
}

void Board::generateRookMoves(MoveList& moves) const {
    Bitboard rooks = pieces_[sideToMove_][ROOK];
    constexpr int delta[4] = {1, -1, 8, -8};

    while (rooks) {
        const Square from = static_cast<Square>(std::countr_zero(rooks));
        rooks &= rooks - 1;
        const int file = fileof(from);
        const int rank = rankof(from);
        const int limit[4] = {7 - file, file, 7 - rank, rank};

        for (int direction = 0; direction < 4; ++direction) {
            for (int step = 1; step <= limit[direction]; ++step) {
                const Square to = static_cast<Square>(
                    static_cast<int>(from) + step * delta[direction]);
                const Bitboard bit = squareBit(to);

                if (occupancy_[sideToMove_] & bit)
                    break;

                moves.add(encodeMove(from, to));
                if (occupancyAll_ & bit)
                    break;
            }
        }
    }
}

void Board::generateQueenMoves(MoveList& moves) const {
    Bitboard queens = pieces_[sideToMove_][QUEEN];
    constexpr int delta[8] = {1, -1, 8, -8, 9, 7, -7, -9};

    while (queens) {
        const Square from = static_cast<Square>(std::countr_zero(queens));
        queens &= queens - 1;
        const int file = fileof(from);
        const int rank = rankof(from);
        const int limit[8] = {
            7 - file, file, 7 - rank, rank,
            std::min(7 - file, 7 - rank), std::min(file, 7 - rank),
            std::min(7 - file, rank), std::min(file, rank)
        };

        for (int direction = 0; direction < 8; ++direction) {
            for (int step = 1; step <= limit[direction]; ++step) {
                const Square to = static_cast<Square>(
                    static_cast<int>(from) + step * delta[direction]);
                const Bitboard bit = squareBit(to);

                if (occupancy_[sideToMove_] & bit)
                    break;

                moves.add(encodeMove(from, to));
                if (occupancyAll_ & bit)
                    break;
            }
        }
    }
}

void Board::generateKingMoves(MoveList&) const {

}

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

void Board::updateSliderBlockers(Colour colour) {
    // TODO: From colour's king, find aligned enemy rook/queen and bishop/queen "snipers".
    // Count occupied squares between each sniper and king. With exactly one blocker,
    // mark blockersForKing_[colour]; if it is colour's piece, mark pinners_[opponent].
}

void Board::setPins() {
    // TODO: Rebuild both colours' slider blockers/pinners and the current side's
    // checkers. Refresh this cache whenever FEN, makeMove, or undoMove changes the board.
    // Stockfish keeps this information in its per-position state.
}

bool Board::legalCandidate(Move) const {
    // TODO: Test exceptional pseudo-legal moves: king destinations after removing
    // the king from its origin, castling transit squares, and en passant with the
    // captured pawn removed (which may expose a rook/bishop line).
    // For pinned non-king pieces, permit only moves along the king-pinner line.
    return false; // Placeholder; generateMoves does not call this yet.
}

void Board::generateMoves(MoveList&) const {
    MoveList pseudo;
    // TODO: Generate ordinary candidates unless in check. In single check, keep
    // captures of the checker or moves onto its blocking ray; in double check,
    // generate only king moves. Stockfish filters king, pinned-piece and en passant
    // candidates through its legal() check; other pseudo-legal moves are accepted.
}

void Board::makeMove(Move) {
    // TODO: Update pieces, mailbox, occupancy and reversible state, then refresh
    // checkers/blockers/pinners for the resulting position.
}

void Board::undoMove(Move) {
    // TODO: Restore the previous position and its cached check/pin information,
    // either from history or by rebuilding it.
}

bool Board::inCheck() const {
    // TODO: Once setPins maintains checkers_, this is simply checkers_ != 0.
    return false;
}

// IGNORES DEFENDING KING AS DEFENDING KING IS NEVER AN EFFECTIVE BLOCKER
// CAN BE REFACTORED LATOR, OR TAKEN IN THROUGH A TEMPLATE IF REQUIRED
// IGNORES EN PASSANT
// UNDEFINED BEHAVIOUR WHEN SQUARE IS OCCUPIED BY ATTACKING SIDE PIECE
bool Board::isSquareAttacked(Square s, Colour attackingSideColour) const {
    int file = static_cast<int>(s) % NUM_FILE;
    int rank = static_cast<int>(s) / NUM_FILE;
    Bitboard BB_Sqr = (Bitboard{1} << static_cast<int>(s));

    Bitboard PlusMask = pieces_[attackingSideColour][ROOK] | pieces_[attackingSideColour][QUEEN];
    Bitboard CrossMask = pieces_[attackingSideColour][BISHOP] | pieces_[attackingSideColour][QUEEN];

    Bitboard Blockers = occupancyAll_ & (~pieces_[~attackingSideColour][KING]);

    auto check_dir = [Blockers, BB_Sqr]<bool positive>(int dir, int limit, Bitboard mask){
        int t = dir;
        while (limit--){
            Bitboard cur = positive ? BB_Sqr << t : BB_Sqr >> t;
            if (cur & mask){return true;}
            if (cur & Blockers){return false;}
            t += dir;
        }
        return false;
    };

    if (check_dir.operator()<true>(8, 7-rank, PlusMask)){return true;}
    if (check_dir.operator()<false>(8, rank, PlusMask)){return true;}
    if (check_dir.operator()<true>(1, 7-file, PlusMask)){return true;}
    if (check_dir.operator()<false>(1, file, PlusMask)){return true;}

    if (check_dir.operator()<true>(9, std::min(7-rank, 7-file), CrossMask)){return true;}
    if (check_dir.operator()<false>(9, std::min(rank, file), CrossMask)){return true;}
    if (check_dir.operator()<true>(7, std::min(7-rank, file), CrossMask)){return true;}
    if (check_dir.operator()<false>(7, std::min(rank, 7-file), CrossMask)){return true;}

    const Bitboard oppKnight = pieces_[attackingSideColour][KNIGHT];
    auto check = [oppKnight](Bitboard square, bool fileCheck){return static_cast<bool>(square & oppKnight) && fileCheck;};

    if (check(BB_Sqr << 17, file < 7)){return true;}
    if (check(BB_Sqr << 15, file > 0)){return true;}
    if (check(BB_Sqr << 10, file < 6)){return true;}
    if (check(BB_Sqr <<  6, file > 1)){return true;}
    if (check(BB_Sqr >> 17, file > 0)){return true;}
    if (check(BB_Sqr >> 15, file < 7)){return true;}
    if (check(BB_Sqr >> 10, file > 1)){return true;}
    if (check(BB_Sqr >>  6, file < 6)){return true;}

    const Bitboard pawns = pieces_[attackingSideColour][PAWN];
    const Bitboard oppPawn = attackingSideColour == WHITE ?
        ((pawns & ~BB_FILE_A) << 7) | ((pawns & ~BB_FILE_H) << 9):
        ((pawns & ~BB_FILE_A) >> 9) | ((pawns & ~BB_FILE_H) >> 7);

    if (oppPawn & BB_Sqr){return true;}

    const Bitboard oppKing = pieces_[attackingSideColour][KING];
    const Bitboard oppKingHor = ((oppKing & ~BB_FILE_H) << 1) | ((oppKing & ~BB_FILE_A) >> 1) | oppKing;
    const Bitboard oppKingAll = ((oppKingHor << 8) | (oppKingHor >> 8) | oppKingHor);

    return (oppKingAll & BB_Sqr);
}

Square Board::enPassantSquare() const {
    if (!hasEnPassant())
        return NO_SQUARE;

    Rank rank = sideToMove_ == WHITE ? RANK_6 : RANK_3;
    return makeSquare(enPassantFile(), rank);
}
