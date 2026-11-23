/*
 * Copyright 2026 Alan Crispin <crispinalan@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef CHESSPOSITION_H
#define CHESSPOSITION_H

#include <string>

class ChessPosition
{
public:
    enum class Color
    {
        White,
        Black
    };

    enum class PieceType
    {
        None,
        Pawn,
        Knight,
        Bishop,
        Rook,
        Queen,
        King
    };

    struct Piece
    {
        Color color = Color::White;
        PieceType type = PieceType::None;

        bool isEmpty() const
        {
            return type == PieceType::None;
        }
    };

    struct Move
    {
        int fromRow = 0;
        int fromCol = 0;
        int toRow = 0;
        int toCol = 0;
    };

    struct Square
    {
        int row = -1;
        int col = -1;

        bool isInvalid() const
        {
            return row < 0 || col < 0;
        }
    };

    Piece board[8][8]{};

    Color sideToMove = Color::White;

    bool canCastleWhiteKingSide = true;
    bool canCastleWhiteQueenSide = true;
    bool canCastleBlackKingSide = true;
    bool canCastleBlackQueenSide = true;

    Square enPassantTarget;

    std::string key() const;


    void resetToStart();
};

#endif // CHESSPOSITION_H

