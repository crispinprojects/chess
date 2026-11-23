/*
 * Copyright 2026 Alan Crispin <crispinalan@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef CHESSENGINE_H
#define CHESSENGINE_H

#include "chessposition.h"

#include <vector>

class ChessEngine
{
public:
    using Color = ChessPosition::Color;
    using PieceType = ChessPosition::PieceType;
    using Piece = ChessPosition::Piece;
    using Move = ChessPosition::Move;
    using Square = ChessPosition::Square;

    struct SavedMove
    {
        Piece from;
        Piece to;
        Piece captured;

        PieceType promotion = PieceType::None;

        int capturedRow = -1;
        int capturedCol = -1;

        bool enPassant = false;
        bool castle = false;

        int rookFromRow = -1;
        int rookFromCol = -1;
        int rookToRow = -1;
        int rookToCol = -1;
        Piece rook;

        bool canCastleWhiteKingSide = true;
        bool canCastleWhiteQueenSide = true;
        bool canCastleBlackKingSide = true;
        bool canCastleBlackQueenSide = true;

        Square enPassantTarget;
    };

    // Basic state queries
    static Color opponent(Color c);
    static bool isOnBoard(int row, int col);

    static bool findKing(const ChessPosition &p, Color color, int &row, int &col);
    static bool isSquareAttacked(const ChessPosition &p, int row, int col, Color attacker);
    static bool isInCheck(const ChessPosition &p, Color color);

    // Move generation
    static void generatePseudoLegalMoves(const ChessPosition &p,
                                         int row,
                                         int col,
                                         std::vector<Move> &out);

    static bool moveLeavesKingInCheck(const ChessPosition &p,
                                      int fromRow,
                                      int fromCol,
                                      int toRow,
                                      int toCol);

    static std::vector<Move> legalMoves(const ChessPosition &p, int row, int col);
    static std::vector<Move> legalMovesForColor(const ChessPosition &p, Color color);
    static bool hasAnyLegalMove(const ChessPosition &p, Color color);

    // Evaluation
    static int pieceValue(PieceType t);
    static int evaluate(const ChessPosition &p);

    // Move apply / undo
    static SavedMove applyMove(ChessPosition &p,
                               int fromRow,
                               int fromCol,
                               int toRow,
                               int toCol);

    static void undoMove(ChessPosition &p,
                         const SavedMove &saved,
                         int fromRow,
                         int fromCol,
                         int toRow,
                         int toCol);

    // Search
    static Move findBestMove(const ChessPosition &start,
                             Color aiColor,
                             int depth);

    static bool isInsufficientMaterial(const ChessPosition &p);





private:
    static bool findKingIn(const Piece b[8][8], Color color, int &row, int &col);

    static bool isSquareAttackedIn(const Piece b[8][8],
                                   int row,
                                   int col,
                                   Color attacker);

    static void addStraightMoves(const ChessPosition &p,
                                 int row,
                                 int col,
                                 int dr,
                                 int dc,
                                 Color color,
                                 std::vector<Move> &out);

    static bool isEnPassantMove(const ChessPosition &p,
                                int fromRow,
                                int fromCol,
                                int toRow,
                                int toCol,
                                const Piece &moving);

    static bool isCastleMove(const ChessPosition &p,
                             int fromRow,
                             int fromCol,
                             int toRow,
                             int toCol,
                             const Piece &moving);

    static void updateCastlingRightsAfterMove(
        ChessPosition &p,
        int fromRow,
        int fromCol,
        int toRow,
        int toCol,
        const Piece &moving,
        int capturedRow,
        int capturedCol,
        const Piece &captured,
        bool castle);

    static int moveScore(const ChessPosition &p, const Move &m);

    static int piecePositionBonus(PieceType t, int row, int col, Color color);

    static int minimax(ChessPosition &p,
                       Color toMove,
                       int depth,
                       int alpha,
                       int beta);
};

#endif // CHESSENGINE_H

