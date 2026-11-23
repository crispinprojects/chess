/*
 * Copyright 2026 Alan Crispin <crispinalan@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "chessposition.h"

/**
 * @brief Resets the position to the standard chess starting position.
 *
 * @details
 * This method restores the initial piece placement, sets White to move,
 * restores all castling rights, and clears the en-passant target.
 *
 * The position object is deliberately pure C++ and knows nothing about:
 * - move history,
 * - undo state,
 * - repetition counts,
 * - game result,
 * - AI state,
 * - or GUI state.
 *
 * Those are owned by higher-level objects such as ChessBoardWidget.
 *
 * @note
 * This method does not reset move history or repetition counters.
 * The widget should call this and then reset its own game state separately.
 */
void ChessPosition::resetToStart()
{
    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            board[r][c] = Piece{};
        }
    }

    // Black back rank
    board[0][0] = { Color::Black, PieceType::Rook };
    board[0][1] = { Color::Black, PieceType::Knight };
    board[0][2] = { Color::Black, PieceType::Bishop };
    board[0][3] = { Color::Black, PieceType::Queen };
    board[0][4] = { Color::Black, PieceType::King };
    board[0][5] = { Color::Black, PieceType::Bishop };
    board[0][6] = { Color::Black, PieceType::Knight };
    board[0][7] = { Color::Black, PieceType::Rook };

    // Black pawns
    for (int c = 0; c < 8; ++c)
    {
        board[1][c] = { Color::Black, PieceType::Pawn };
    }

    // White pawns
    for (int c = 0; c < 8; ++c)
    {
        board[6][c] = { Color::White, PieceType::Pawn };
    }

    // White back rank
    board[7][0] = { Color::White, PieceType::Rook };
    board[7][1] = { Color::White, PieceType::Knight };
    board[7][2] = { Color::White, PieceType::Bishop };
    board[7][3] = { Color::White, PieceType::Queen };
    board[7][4] = { Color::White, PieceType::King };
    board[7][5] = { Color::White, PieceType::Bishop };
    board[7][6] = { Color::White, PieceType::Knight };
    board[7][7] = { Color::White, PieceType::Rook };

    sideToMove = Color::White;

    canCastleWhiteKingSide = true;
    canCastleWhiteQueenSide = true;
    canCastleBlackKingSide = true;
    canCastleBlackQueenSide = true;

    enPassantTarget = Square{-1, -1};
}

/**
 * @brief Creates a stable text key representing the exact position.
 *
 * @details
 * The key is used to detect repeated positions for the threefold
 * repetition draw rule.
 *
 * The encoded key includes:
 * - side to move,
 * - castling rights,
 * - en-passant target,
 * - full board placement.
 *
 * Two positions are considered the same if their keys are identical.
 *
 * @return
 * A standard C++ string representing the position.
 *
 * @note
 * The exact string format is an implementation detail. It only needs to be:
 * - deterministic,
 * - unique for different positions,
 * - safe to use as a std::map key.
 *
 * @note
 * This method is pure C++
 */
std::string ChessPosition::key() const
{
    std::string s;

    // Side to move
    char side = 'B';
    if (sideToMove == Color::White)
        side = 'W';
    s += side;

    // Castling rights
    if (canCastleWhiteKingSide)
        s += '1';
    else
        s += '0';

    if (canCastleWhiteQueenSide)
        s += '1';
    else
        s += '0';

    if (canCastleBlackKingSide)
        s += '1';
    else
        s += '0';

    if (canCastleBlackQueenSide)
        s += '1';
    else
        s += '0';

    // En-passant target
    //
    // row/col can be -1, 0, 1, ... 7
    // We encode -1 as '0', 0 as '1', ..., 7 as '8'
    s += static_cast<char>('0' + (enPassantTarget.row + 1));
    s += static_cast<char>('0' + (enPassantTarget.col + 1));

    // Board
    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            const Piece &p = board[r][c];

            if (p.isEmpty())
            {
                s += '-';
                continue;
            }

            char color = 'B';
            if (p.color == Color::White)
                color = 'W';
            s += color;

            char type = '0';

            switch (p.type)
            {
                case PieceType::Pawn:
                    type = '1';
                    break;

                case PieceType::Knight:
                    type = '2';
                    break;

                case PieceType::Bishop:
                    type = '3';
                    break;

                case PieceType::Rook:
                    type = '4';
                    break;

                case PieceType::Queen:
                    type = '5';
                    break;

                case PieceType::King:
                    type = '6';
                    break;

                case PieceType::None:
                    type = '0';
                    break;
            }

            s += type;
        }
    }

    return s;
}


