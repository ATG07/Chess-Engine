#include <ranges>
#include <algorithm>
#include <bit>
#include <charconv>
#include <stdexcept>
#include "board.hpp"
#include "zobrist.hpp"

Board::Board(){
    // Should be enough for now ig?
    board_.fill(NO_PIECE);
    state_ = 0;
    history_.reserve(256);
}

void Board::addPiece(Piece piece, Square square) {
    board_[square] = piece;
    zobristKey_ ^= Zobrist::piece[piece][square];

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
    zobristKey_ ^= Zobrist::piece[piece][square];

    Colour colour = getColour(piece);
    PieceType pieceType = getPieceType(piece);
    
    pieces_[colour][pieceType]  &= ~squareBit(square);
    occupancy_[colour]          &= ~squareBit(square);
    occupancyAll_               &= ~squareBit(square);
}

void Board::movePiece(Piece piece, Square from, Square to) {
    board_[from] = NO_PIECE;
    board_[to] = piece;

    zobristKey_ ^= Zobrist::piece[piece][to];
    zobristKey_ ^= Zobrist::piece[piece][from];

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

bool Board::followsPin(Square from, Square to) const {
    if (!(pinnedToKing(sideToMove_) & squareBit(from)))
        return true;

    const Square king = static_cast<Square>(
        std::countr_zero(pieces_[sideToMove_][KING]));
    const int fromFile = fileof(from) - fileof(king);
    const int fromRank = rankof(from) - rankof(king);
    const int toFile = fileof(to) - fileof(king);
    const int toRank = rankof(to) - rankof(king);
    return fromFile * toRank == fromRank * toFile;
}

bool Board::legalEnPassant(Square from, Square to) const {
    const Square king = static_cast<Square>(
        std::countr_zero(pieces_[sideToMove_][KING]));
    const Square captured = static_cast<Square>(
        static_cast<int>(to) + (sideToMove_ == WHITE ? -8 : 8));

    Bitboard occupied = occupancyAll_;
    occupied &= ~squareBit(from);
    occupied &= ~squareBit(captured);
    occupied |= squareBit(to);

    const Bitboard straight = pieces_[~sideToMove_][ROOK] |
                              pieces_[~sideToMove_][QUEEN];
    const Bitboard diagonal = pieces_[~sideToMove_][BISHOP] |
                              pieces_[~sideToMove_][QUEEN];
    constexpr int fileStep[8] = {1, -1, 0, 0, 1, -1, 1, -1};
    constexpr int rankStep[8] = {0, 0, 1, -1, 1, 1, -1, -1};

    for (int direction = 0; direction < 8; ++direction) {
        int file = fileof(king) + fileStep[direction];
        int rank = rankof(king) + rankStep[direction];

        while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
            const Bitboard square = squareBit(makeSquare(
                static_cast<File>(file), static_cast<Rank>(rank)));

            if (occupied & square) {
                const Bitboard sliders = direction < 4 ? straight : diagonal;
                if (sliders & square)
                    return false;
                break;
            }

            file += fileStep[direction];
            rank += rankStep[direction];
        }
    }
    return true;
}

// sideToMove REPEATEDLY BRANCHES, TRY AND EXPLICITLY USE CONSTEXPR
void Board::generatePawnMoves(MoveList& moves, Bitboard targets) const {
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

    auto addMove = [&]<MoveType type>(Bitboard destinations, int offset) {
        destinations &= targets;
        while (destinations) {
            const Square to = static_cast<Square>(std::countr_zero(destinations));
            const Square from = static_cast<Square>(static_cast<int>(to) - offset);
            destinations &= destinations - 1;

            if (!followsPin(from, to))
                continue;

            if constexpr (type == PROMOTION) {
                for (int piece = KNIGHT; piece <= QUEEN; ++piece)
                    moves.add(encodeMove(from, to, PROMOTION,
                                         static_cast<PieceType>(piece)));
            } else {
                moves.add(encodeMove(from, to, type));
            }
        }
    };

    auto splitPromotion = [&](Bitboard destinations, int offset) {
        addMove.operator()<NORMAL>(destinations & ~promotionRank, offset);
        addMove.operator()<PROMOTION>(destinations & promotionRank, offset);
    };

    splitPromotion(single_move, forward);
    addMove.operator()<NORMAL>(double_move, 2 * forward);
    splitPromotion(left_capture, sideToMove_ == WHITE ? 7 : -9);
    splitPromotion(right_capture, sideToMove_ == WHITE ? 9 : -7);

    if (hasEnPassant()) {
        const Square to = enPassantSquare();
        const Bitboard pawns = pieces_[sideToMove_][PAWN];

        const int leftFrom = static_cast<int>(to) -
                             (sideToMove_ == WHITE ? 7 : -9);
        const int rightFrom = static_cast<int>(to) -
                              (sideToMove_ == WHITE ? 9 : -7);
        const Square captured = static_cast<Square>(
            static_cast<int>(to) + (sideToMove_ == WHITE ? -8 : 8));
        const bool evades = (targets & squareBit(to)) ||
                            (checkers_ & squareBit(captured));

        const Square left = static_cast<Square>(leftFrom);
        const Square right = static_cast<Square>(rightFrom);

        if (evades && fileof(to) != FILE_H && (pawns & squareBit(left)) &&
            followsPin(left, to) && legalEnPassant(left, to))
            moves.add(encodeMove(left, to, EN_PASSANT));
        if (evades && fileof(to) != FILE_A && (pawns & squareBit(right)) &&
            followsPin(right, to) && legalEnPassant(right, to))
            moves.add(encodeMove(right, to, EN_PASSANT));
    }
}

void Board::generateKnightMoves(MoveList& moves, Bitboard targets) const {
    constexpr Bitboard notA = ~BB_FILE_A;
    constexpr Bitboard notH = ~BB_FILE_H;
    constexpr Bitboard notAB = ~(BB_FILE_A | (BB_FILE_A << 1));
    constexpr Bitboard notGH = ~(BB_FILE_H | (BB_FILE_H >> 1));
    const Bitboard knights = pieces_[sideToMove_][KNIGHT] &
                             ~pinnedToKing(sideToMove_);

    auto emit = [&](Bitboard destinations, int offset) {
        destinations &= ~occupancy_[sideToMove_] & targets;
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

void Board::generateBishopMoves(MoveList& moves, Bitboard targets) const {
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
            if (!limit[direction] ||
                !followsPin(from, static_cast<Square>(
                    static_cast<int>(from) + delta[direction])))
                continue;

            for (int step = 1; step <= limit[direction]; ++step) {
                const Square to = static_cast<Square>(
                    static_cast<int>(from) + step * delta[direction]);
                const Bitboard bit = squareBit(to);

                if (occupancy_[sideToMove_] & bit)
                    break;

                if (targets & bit)
                    moves.add(encodeMove(from, to));
                if (occupancyAll_ & bit)
                    break;
            }
        }
    }
}

void Board::generateRookMoves(MoveList& moves, Bitboard targets) const {
    Bitboard rooks = pieces_[sideToMove_][ROOK];
    constexpr int delta[4] = {1, -1, 8, -8};

    while (rooks) {
        const Square from = static_cast<Square>(std::countr_zero(rooks));
        rooks &= rooks - 1;
        const int file = fileof(from);
        const int rank = rankof(from);
        const int limit[4] = {7 - file, file, 7 - rank, rank};

        for (int direction = 0; direction < 4; ++direction) {
            if (!limit[direction] ||
                !followsPin(from, static_cast<Square>(
                    static_cast<int>(from) + delta[direction])))
                continue;

            for (int step = 1; step <= limit[direction]; ++step) {
                const Square to = static_cast<Square>(
                    static_cast<int>(from) + step * delta[direction]);
                const Bitboard bit = squareBit(to);

                if (occupancy_[sideToMove_] & bit)
                    break;

                if (targets & bit)
                    moves.add(encodeMove(from, to));
                if (occupancyAll_ & bit)
                    break;
            }
        }
    }
}

void Board::generateQueenMoves(MoveList& moves, Bitboard targets) const {
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
            if (!limit[direction] ||
                !followsPin(from, static_cast<Square>(
                    static_cast<int>(from) + delta[direction])))
                continue;

            for (int step = 1; step <= limit[direction]; ++step) {
                const Square to = static_cast<Square>(
                    static_cast<int>(from) + step * delta[direction]);
                const Bitboard bit = squareBit(to);

                if (occupancy_[sideToMove_] & bit)
                    break;

                if (targets & bit)
                    moves.add(encodeMove(from, to));
                if (occupancyAll_ & bit)
                    break;
            }
        }
    }
}

void Board::generateKingMoves(MoveList& moves) const {
    // Normal 8 moves

    const Bitboard cur = pieces_[sideToMove_][KING];
    const Square s1 = static_cast<Square>(std::countr_zero(cur));
    const Bitboard hor = ((cur&~BB_FILE_A) >> 1) | ((cur&~BB_FILE_H) << 1);
    Bitboard to = (hor << 8) | (hor >> 8) | hor | (cur >> 8) | (cur << 8);
    to &= ~occupancy_[sideToMove_];

    while (to){
        Square s2 = static_cast<Square>(std::countr_zero(to));
        to = to & (to - 1);

        if (!isSquareAttacked(s2, ~sideToMove_)){
            moves.add(encodeMove(s1, s2));
        }
    }

    // Castling
    std::uint8_t mask = sideToMove_ == WHITE ? 1 : (1 << 2);

    if (state_ & mask){     // KINGSIDE castling
        auto areSquaresEmpty = [&]{
            const Bitboard emptySquares = Bitboard{3} << (sideToMove_ == WHITE ? 5 : 5 + 8 * 7);
            return !static_cast<bool>(occupancyAll_ & emptySquares);
        };

        auto noAttacks = [&](){
            return  (!isSquareAttacked(s1, ~sideToMove_)) &&
                    (!isSquareAttacked(static_cast<Square>(static_cast<int>(s1) + 1), ~sideToMove_)) &&
                    (!isSquareAttacked(static_cast<Square>(static_cast<int>(s1) + 2), ~sideToMove_));
        };

        if (areSquaresEmpty() && noAttacks()){
            moves.add(encodeMove(
                s1,
                static_cast<Square>(static_cast<int>(s1) + 3),
                CASTLING    
            ));
        }
    }

    mask <<= 1;

    if (state_ & mask){    //QUEENSIDE castling
        
        auto areSquaresEmpty = [&]{
            const Bitboard emptySquares = Bitboard{7} << (sideToMove_ == WHITE ? 1 : 1 + 8 * 7);
            return !static_cast<bool>(occupancyAll_ & emptySquares);
        };

        auto noAttacks = [&](){
            return  (!isSquareAttacked(s1, ~sideToMove_)) &&
                    (!isSquareAttacked(static_cast<Square>(static_cast<int>(s1) - 1), ~sideToMove_)) &&
                    (!isSquareAttacked(static_cast<Square>(static_cast<int>(s1) - 2), ~sideToMove_));
        };

        if (areSquaresEmpty() && noAttacks()){
            moves.add(encodeMove(
                s1,
                static_cast<Square>(static_cast<int>(s1) - 4),
                CASTLING
            ));
        }
    }
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
    history_.clear();

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

    zobristKey_ ^= Zobrist::castling[castlingRights()];
    if (hasEnPassant())
        zobristKey_ ^= Zobrist::enpassant[enPassantFile()];
    if (sideToMove_ == BLACK)
        zobristKey_ ^= Zobrist::side;

    setPinsAttackers();
}

void Board::updateSliderBlockers(Colour colour) {
    // TODO: From colour's king, find aligned enemy rook/queen and bishop/queen "snipers".
    // Count occupied squares between each sniper and king. With exactly one blocker,
    // mark blockersForKing_[colour]; if it is colour's piece, mark pinners_[opponent].
}

void Board::setPinsAttackers() {
    const Colour attackingSideColour = ~sideToMove_;
    Square s = static_cast<Square>(std::countr_zero(pieces_[sideToMove_][KING]));

    checkers_ = 0;
    blockersForKing_[sideToMove_] = 0;
    pinners_[~sideToMove_] = 0;

    int file = static_cast<int>(s) % NUM_FILE;
    int rank = static_cast<int>(s) / NUM_FILE;
    Bitboard BB_Sqr = (Bitboard{1} << static_cast<int>(s));

    Bitboard PlusMask = pieces_[attackingSideColour][ROOK] | pieces_[attackingSideColour][QUEEN];
    Bitboard CrossMask = pieces_[attackingSideColour][BISHOP] | pieces_[attackingSideColour][QUEEN];

    Bitboard Blockers = occupancyAll_ & (~pieces_[sideToMove_][KING]);

    //MAYBE dir COULD ALSO BE MAKE INTO A TEMPLATE ARGUMENT
    auto check_dir = [&]<bool positive>(int dir, int limit, Bitboard mask){
        int t = dir;
        Bitboard blockingPiece = Bitboard{0};

        while (limit--){
            const Bitboard cur = [&]{
                if constexpr (positive) {return BB_Sqr << t;}
                else                    {return BB_Sqr >> t;}
            }();

            if ((cur & mask)){
                if (static_cast<bool>(blockingPiece)){
                    blockersForKing_[sideToMove_] |= blockingPiece;
                    pinners_[attackingSideColour] |= (blockingPiece & occupancy_[sideToMove_]) ? cur : 0;
                    return;
                }
                else{
                    checkers_ |= cur;
                    return;
                }
            }

            if (cur & Blockers){
                if (static_cast<bool>(blockingPiece)){
                    return;
                }
                blockingPiece = cur;
            }

            t += dir;
        }
        return;
    };

    check_dir.operator()<true>(8, 7-rank, PlusMask);
    check_dir.operator()<false>(8, rank, PlusMask);
    check_dir.operator()<true>(1, 7-file, PlusMask);
    check_dir.operator()<false>(1, file, PlusMask);

    check_dir.operator()<true>(9, std::min(7-rank, 7-file), CrossMask);
    check_dir.operator()<false>(9, std::min(rank, file), CrossMask);
    check_dir.operator()<true>(7, std::min(7-rank, file), CrossMask);
    check_dir.operator()<false>(7, std::min(rank, 7-file), CrossMask);

    const Bitboard oppKnight = pieces_[attackingSideColour][KNIGHT];
    auto check = [oppKnight](Bitboard square, bool fileCheck){return static_cast<bool>(square & oppKnight) && fileCheck;};

    if (check(BB_Sqr << 17, file < 7)){checkers_ |= BB_Sqr << 17;}
    if (check(BB_Sqr << 15, file > 0)){checkers_ |= BB_Sqr << 15;}
    if (check(BB_Sqr << 10, file < 6)){checkers_ |= BB_Sqr << 10;}
    if (check(BB_Sqr <<  6, file > 1)){checkers_ |= BB_Sqr <<  6;}
    if (check(BB_Sqr >> 17, file > 0)){checkers_ |= BB_Sqr >> 17;}
    if (check(BB_Sqr >> 15, file < 7)){checkers_ |= BB_Sqr >> 15;}
    if (check(BB_Sqr >> 10, file > 1)){checkers_ |= BB_Sqr >> 10;}
    if (check(BB_Sqr >>  6, file < 6)){checkers_ |= BB_Sqr >>  6;}

    // ONLY ONE PAWN CAN CHECK A KING AT ANY GIVEN POINT
    const Bitboard pawns = pieces_[attackingSideColour][PAWN];
    const Bitboard oppPawnCheck = attackingSideColour == WHITE ?
    ((BB_Sqr & ~BB_FILE_H) >> 7) | ((BB_Sqr & ~BB_FILE_A) >> 9):
    ((BB_Sqr & ~BB_FILE_H) << 9) | ((BB_Sqr & ~BB_FILE_A) << 7);

    checkers_ |= (oppPawnCheck & pawns);

    // SKIP KING BECAUSE KING CAN'T CHECK A KING
}

// REQUIRES PINS AND ATTACKERS TO BE SET
void Board::generateMoves(MoveList& moves) const {
    moves.clear();  // REDUNDANT
    generateKingMoves(moves);

    const int checks = std::popcount(checkers_);
    if (checks >= 2)
        return;

    Bitboard targets = ~Bitboard{0};

    if (checks == 1){    // MUST BLOCK OR CAPTURE OR MOVE KING
        const Square king = static_cast<Square>(
            std::countr_zero(pieces_[sideToMove_][KING]));
        const Square checker = static_cast<Square>(std::countr_zero(checkers_));
        targets = squareBit(checker);

        const PieceType type = getPieceType(board_[checker]);
        if (type == BISHOP || type == ROOK || type == QUEEN) {
            int file = fileof(king);
            int rank = rankof(king);
            const int checkerFile = fileof(checker);
            const int checkerRank = rankof(checker);
            const int fileStep = (checkerFile > file) - (checkerFile < file);
            const int rankStep = (checkerRank > rank) - (checkerRank < rank);

            file += fileStep;
            rank += rankStep;
            while (file != checkerFile || rank != checkerRank) {
                targets |= squareBit(makeSquare(
                    static_cast<File>(file), static_cast<Rank>(rank)));
                file += fileStep;
                rank += rankStep;
            }
        }
    }

    generatePawnMoves(moves, targets);
    generateKnightMoves(moves, targets);
    generateBishopMoves(moves, targets);
    generateRookMoves(moves, targets);
    generateQueenMoves(moves, targets);
}

void Board::makeMove(Move move) {
    const Square source = moveFrom(move);
    const Square destination = moveTo(move);

    const MoveType type = moveType(move);
    const PieceType promotionPiece = promotionType(move);

    const Piece pieceToMove = board_[static_cast<int>(source)];
    const Piece pieceAtDest = board_[static_cast<int>(destination)];

    history_.push_back(UndoState(
        move,
        pieceAtDest,
        state_,
        halfmoveClock_,
        zobristKey_
    ));

    zobristKey_ ^= Zobrist::castling[state_ & 0b1111];
    if (hasEnPassant())
    zobristKey_ ^= Zobrist::enpassant[enPassantFile()];
    zobristKey_ ^= Zobrist::side;

    removePiece(pieceToMove, source);
    if (pieceAtDest != NO_PIECE){
        removePiece(pieceAtDest, destination);
    }

    if (type == PROMOTION){
        Piece newPiece;
        if (sideToMove_ == WHITE){
            newPiece = static_cast<Piece>(promotionPiece);
        }
        else{
            newPiece = static_cast<Piece>(static_cast<int>(promotionPiece) + static_cast<int>(NUM_PIECE_TYPE));
        }
        addPiece(newPiece, destination);
    }
    else if (type == CASTLING){
        const Square newKingSqr = static_cast<Square>
                            (static_cast<int>(destination) > static_cast<int>(source) ?
                            static_cast<int>(source) + 2:
                            static_cast<int>(source) - 2);
        const Square newRookSqr = static_cast<Square>
                            (static_cast<int>(destination) > static_cast<int>(source) ?
                            static_cast<int>(source) + 1:
                            static_cast<int>(source) - 1);

        addPiece(pieceToMove, newKingSqr);
        addPiece(pieceAtDest, newRookSqr);
    }
    else{
        addPiece(pieceToMove, destination);
    }

    if (type == EN_PASSANT){
        const Square oppPawnSqr = static_cast<Square>(
            static_cast<int>(destination) +
            ((sideToMove_ == WHITE) ? -8 : 8)
        );

        const Piece oppPawn = board_[oppPawnSqr];

        removePiece(oppPawn, oppPawnSqr);
    }

    if (pieceToMove == W_KING){state_ &= 0b0000'1100;}
    if (pieceToMove == B_KING){state_ &= 0b0000'0011;}
    if (pieceToMove == W_ROOK && source == H1){state_ &= 0b0000'1110;}
    if (pieceToMove == W_ROOK && source == A1){state_ &= 0b0000'1101;}
    if (pieceToMove == B_ROOK && source == H8){state_ &= 0b0000'1011;}
    if (pieceToMove == B_ROOK && source == A8){state_ &= 0b0000'0111;}
    if (destination == H1){state_ &= 0b0000'1110;}
    if (destination == A1){state_ &= 0b0000'1101;}
    if (destination == H8){state_ &= 0b0000'1011;}
    if (destination == A8){state_ &= 0b0000'0111;}
    state_ &= 0b0000'1111;
    if (pieceToMove == W_PAWN && destination - source == 16){
        state_ ^= (fileof(destination) << 4) + (1 << 7);
    }
    if (pieceToMove == B_PAWN && source - destination == 16){
        state_ ^= (fileof(destination) << 4) + (1 << 7);
    }

    if (sideToMove_ == BLACK){
        fullmoveNumber_++;
    }
    if (type != CASTLING &&
    (pieceAtDest != NO_PIECE || pieceToMove == W_PAWN || pieceToMove == B_PAWN || type == EN_PASSANT))
    {
        halfmoveClock_ = 0;
    }
    else{
        halfmoveClock_++;
    }
    sideToMove_ = ~sideToMove_; 
    
    setPinsAttackers();

    zobristKey_ ^= Zobrist::castling[state_ & 0b1111];
    if (hasEnPassant())
    zobristKey_ ^= Zobrist::enpassant[enPassantFile()];
}

void Board::undoMove() {
    // TODO: Read history_.back(), restore the previous position, then pop_back().
    // Restore cached check/pin information from history or by rebuilding it.
}

bool Board::inCheck() const {
    return checkers_ != 0;
}

// IGNORES DEFENDING KING AS DEFENDING KING IS NEVER AN EFFECTIVE BLOCKER
// CAN BE REFACTORED LATOR, OR TAKEN IN THROUGH A TEMPLATE IF REQUIRED
// IGNORES EN PASSANT
// UNDEFINED BEHAVIOUR WHEN SQUARE IS OCCUPIED BY ATTACKING SIDE KING
// PROBABLY NOT TOO MUCH OF AN OPTIMISATION
bool Board::isSquareAttacked(Square s, Colour attackingSideColour) const {
    int file = static_cast<int>(s) % NUM_FILE;
    int rank = static_cast<int>(s) / NUM_FILE;
    Bitboard BB_Sqr = (Bitboard{1} << static_cast<int>(s));

    Bitboard PlusMask = pieces_[attackingSideColour][ROOK] | pieces_[attackingSideColour][QUEEN];
    Bitboard CrossMask = pieces_[attackingSideColour][BISHOP] | pieces_[attackingSideColour][QUEEN];

    Bitboard Blockers = occupancyAll_ & (~pieces_[~attackingSideColour][KING]);

    //MAYBE dir COULD ALSO BE MAKE INTO A TEMPLATE ARGUMENT
    auto check_dir = [Blockers, BB_Sqr]<bool positive>(int dir, int limit, Bitboard mask){
        int t = dir;
        while (limit--){
            const Bitboard cur = [&]{
                if constexpr (positive) {return BB_Sqr << t;}
                else                    {return BB_Sqr >> t;}
            }();
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
