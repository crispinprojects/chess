/*
 * Copyright 2026 Alan Crispin <crispinalan@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "chessengine.h"

#include <algorithm>
#include <vector>
#include <cstdlib>
#include <cmath>

//---------------------------------------------------------------------------
// Static piece square tables
// Piece Square Tables (PST) are written from White’s perspective.
// For Black, mirror vertically: (*table)[7 - row][col]
//---------------------------------------------------------------------------
static const int pawnPST[8][8] =
{
    {   0,   0,   0,   0,   0,   0,   0,   0 },
    {  50,  50,  50,  50,  50,  50,  50,  50 },
    {  10,  10,  20,  30,  30,  20,  10,  10 },
    {   5,   5,  10,  25,  25,  10,   5,   5 },
    {   0,   0,   0,  20,  20,   0,   0,   0 },
    {   5,  -5, -10,   0,   0, -10,  -5,   5 },
    {   5,  10,  10, -20, -20,  10,  10,   5 },
    {   0,   0,   0,   0,   0,   0,   0,   0 }
};

static const int knightPST[8][8] =
{
    { -50, -40, -30, -30, -30, -30, -40, -50 },
    { -40, -20,   0,   0,   0,   0, -20, -40 },
    { -30,   0,  10,  15,  15,  10,   0, -30 },
    { -30,   5,  15,  20,  20,  15,   5, -30 },
    { -30,   0,  15,  20,  20,  15,   0, -30 },
    { -30,   5,  10,  15,  15,  10,   5, -30 },
    { -40, -20,   0,   5,   5,   0, -20, -40 },
    { -50, -40, -30, -30, -30, -30, -40, -50 }
};

static const int bishopPST[8][8] =
{
    { -20, -10, -10, -10, -10, -10, -10, -20 },
    { -10,   0,   0,   0,   0,   0,   0, -10 },
    { -10,   0,   5,  10,  10,   5,   0, -10 },
    { -10,   5,   5,  10,  10,   5,   5, -10 },
    { -10,   0,  10,  10,  10,  10,   0, -10 },
    { -10,  10,  10,  10,  10,  10,  10, -10 },
    { -10,   5,   0,   0,   0,   0,   5, -10 },
    { -20, -10, -10, -10, -10, -10, -10, -20 }
};

static const int rookPST[8][8] =
{
    {   0,   0,   0,   0,   0,   0,   0,   0 },
    {   5,  10,  10,  10,  10,  10,  10,   5 },
    {  -5,   0,   0,   0,   0,   0,   0,  -5 },
    {  -5,   0,   0,   0,   0,   0,   0,  -5 },
    {  -5,   0,   0,   0,   0,   0,   0,  -5 },
    {  -5,   0,   0,   0,   0,   0,   0,  -5 },
    {  -5,   0,   0,   0,   0,   0,   0,  -5 },
    {   0,   0,   0,   5,   5,   0,   0,   0 }
};

static const int queenPST[8][8] =
{
    { -20, -10, -10,  -5,  -5, -10, -10, -20 },
    { -10,   0,   0,   0,   0,   0,   0, -10 },
    { -10,   0,   5,   5,   5,   5,   0, -10 },
    {  -5,   0,   5,   5,   5,   5,   0,  -5 },
    {   0,   0,   5,   5,   5,   5,   0,  -5 },
    { -10,   5,   5,   5,   5,   5,   0, -10 },
    { -10,   0,   5,   0,   0,   0,   0, -10 },
    { -20, -10, -10,  -5,  -5, -10, -10, -20 }
};

static const int kingMiddlegamePST[8][8] =
{
    { -30, -40, -40, -50, -50, -40, -40, -30 },
    { -30, -40, -40, -50, -50, -40, -40, -30 },
    { -30, -40, -40, -50, -50, -40, -40, -30 },
    { -30, -40, -40, -50, -50, -40, -40, -30 },
    { -20, -30, -30, -40, -40, -30, -30, -20 },
    { -10, -20, -20, -20, -20, -20, -20, -10 },
    {  20,  20,   0,   0,   0,   0,  20,  20 },
    {  20,  30,  10,   0,   0,  10,  30,  20 }
};

int ChessEngine::piecePositionBonus(
    PieceType t, int row, int col, Color color)
{
    const int (*table)[8] = nullptr;

    switch (t)
    {
        case PieceType::Pawn:   table = pawnPST; break;
        case PieceType::Knight: table = knightPST; break;
        case PieceType::Bishop: table = bishopPST; break;
        case PieceType::Rook:   table = rookPST; break;
        case PieceType::Queen:  table = queenPST; break;
        case PieceType::King:   table = kingMiddlegamePST; break;
        case PieceType::None:   return 0;
    }

    if (row < 0 || row >= 8 || col < 0 || col >= 8)
        return 0;

    return color == Color::White
    ? table[row][col]
    : table[7 - row][col];
}

// -----------------------------------------------------------------------------
// Basic helpers
// -----------------------------------------------------------------------------

/**
 * @brief Returns the opposite side to move.
 *
 * @param c The current color.
 *
 * @return
 * Color::Black if c is Color::White.
 * Color::White if c is Color::Black.
 *
 * @note
 * This is a pure helper function and is safe to call from any thread.
 */
ChessEngine::Color ChessEngine::opponent(Color c)
{
    return (c == Color::White) ? Color::Black : Color::White;
}

/**
 * @brief Checks whether a square is inside the 8x8 board.
 *
 * @param row Board row, normally 0 to 7.
 * @param col Board column, normally 0 to 7.
 *
 * @return true if the square is on the board, otherwise false.
 *
 * @note
 * This function is used heavily by move generation and attack detection.
 * It is pure C++ and thread-safe as long as the inputs are valid integers.
 */
bool ChessEngine::isOnBoard(int row, int col)
{
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

/**
 * @brief Finds the king of a given color in a raw 8x8 board array.
 *
 * @param b
 * A raw 8x8 board array.
 *
 * @param color
 * The color of the king to find.
 *
 * @param row
 * On success, receives the row of the king.
 *
 * @param col
 * On success, receives the column of the king.
 *
 * @return
 * true if the king was found, otherwise false.
 *
 * @note
 * This low-level helper operates on a raw board array rather than a
 * ChessPosition object. It is used internally by check detection and
 * legality testing.
 *
 * @note
 * This function does not modify the board.
 */
bool ChessEngine::findKingIn(const Piece b[8][8], Color color, int &row, int &col)
{
    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            if (b[r][c].type == PieceType::King && b[r][c].color == color)
            {
                row = r;
                col = c;
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Finds the king of a given color in a ChessPosition.
 *
 * @param p
 * The position to search.
 *
 * @param color
 * The color of the king to find.
 *
 * @param row
 * On success, receives the row of the king.
 *
 * @param col
 * On success, receives the column of the king.
 *
 * @return
 * true if the king was found, otherwise false.
 *
 * @note
 * This is a convenience wrapper around findKingIn() for ChessPosition.
 *
 * @note
 * This function does not modify the position.
 */
bool ChessEngine::findKing(const ChessPosition &p, Color color, int &row, int &col)
{
    return findKingIn(p.board, color, row, col);
}

// -----------------------------------------------------------------------------
// Attack detection
// -----------------------------------------------------------------------------


/**
 * @brief Checks whether a square is attacked by any piece of the given color
 *        in a raw 8x8 board array.
 *
 * @param b
 * A raw 8x8 board array.
 *
 * @param row
 * The row of the square being tested.
 *
 * @param col
 * The column of the square being tested.
 *
 * @param attacker
 * The color of the pieces that may be attacking the square.
 *
 * @return
 * true if at least one piece of the attacker color attacks the square,
 * otherwise false.
 *
 * @details
 * This function handles:
 * - pawn attacks,
 * - knight attacks,
 * - king attacks,
 * - rook/queen straight-line attacks,
 * - bishop/queen diagonal attacks.
 *
 * @note
 * This is attack detection, not legal move generation.
 * It does not verify whether the attacking piece itself is pinned.
 *
 * @note
 * This function is const and does not modify the board.
 */
bool ChessEngine::isSquareAttackedIn(const Piece b[8][8],
                                     int row,
                                     int col,
                                     Color attacker)
{
    // Pawn attacks.
    //
    // Board orientation:
    // row 0 = black back rank
    // row 7 = white back rank
    //
    // White pawns move upward, so a white pawn attacks from row + 1.
    // Black pawns move downward, so a black pawn attacks from row - 1.

    if (attacker == Color::White)
    {
        if (isOnBoard(row + 1, col - 1) &&
            b[row + 1][col - 1].color == Color::White &&
            b[row + 1][col - 1].type == PieceType::Pawn)
        {
            return true;
        }

        if (isOnBoard(row + 1, col + 1) &&
            b[row + 1][col + 1].color == Color::White &&
            b[row + 1][col + 1].type == PieceType::Pawn)
        {
            return true;
        }
    }
    else
    {
        if (isOnBoard(row - 1, col - 1) &&
            b[row - 1][col - 1].color == Color::Black &&
            b[row - 1][col - 1].type == PieceType::Pawn)
        {
            return true;
        }

        if (isOnBoard(row - 1, col + 1) &&
            b[row - 1][col + 1].color == Color::Black &&
            b[row - 1][col + 1].type == PieceType::Pawn)
        {
            return true;
        }
    }

    // Knight attacks.
    static const int knightRow[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int knightCol[8] = {-1,  1, -2,  2, -2, 2, -1, 1};

    for (int i = 0; i < 8; ++i)
    {
        int r = row + knightRow[i];
        int c = col + knightCol[i];

        if (isOnBoard(r, c) &&
            b[r][c].color == attacker &&
            b[r][c].type == PieceType::Knight)
        {
            return true;
        }
    }

    // King attacks.
    for (int dr = -1; dr <= 1; ++dr)
    {
        for (int dc = -1; dc <= 1; ++dc)
        {
            if (dr == 0 && dc == 0)
                continue;

            int r = row + dr;
            int c = col + dc;

            if (isOnBoard(r, c) &&
                b[r][c].color == attacker &&
                b[r][c].type == PieceType::King)
            {
                return true;
            }
        }
    }

    // Rook / Queen straight-line attacks.
    static const int rookDr[4] = {-1, 1, 0, 0};
    static const int rookDc[4] = {0, 0, -1, 1};

    for (int i = 0; i < 4; ++i)
    {
        int r = row + rookDr[i];
        int c = col + rookDc[i];

        while (isOnBoard(r, c))
        {
            const Piece &p = b[r][c];

            if (!p.isEmpty())
            {
                if (p.color == attacker &&
                    (p.type == PieceType::Rook || p.type == PieceType::Queen))
                {
                    return true;
                }

                break;
            }

            r += rookDr[i];
            c += rookDc[i];
        }
    }

    // Bishop / Queen diagonal attacks.
    static const int bishopDr[4] = {-1, -1, 1, 1};
    static const int bishopDc[4] = {-1,  1, -1, 1};

    for (int i = 0; i < 4; ++i)
    {
        int r = row + bishopDr[i];
        int c = col + bishopDc[i];

        while (isOnBoard(r, c))
        {
            const Piece &p = b[r][c];

            if (!p.isEmpty())
            {
                if (p.color == attacker &&
                    (p.type == PieceType::Bishop || p.type == PieceType::Queen))
                {
                    return true;
                }

                break;
            }

            r += bishopDr[i];
            c += bishopDc[i];
        }
    }

    return false;
}

/**
 * @brief Checks whether a square is attacked by any piece of the given color
 *        in a ChessPosition.
 *
 * @param p
 * The position to inspect.
 *
 * @param row
 * The row of the square being tested.
 *
 * @param col
 * The column of the square being tested.
 *
 * @param attacker
 * The color of the pieces that may be attacking the square.
 *
 * @return
 * true if the square is attacked, otherwise false.
 *
 * @note
 * This is a convenience wrapper around isSquareAttackedIn().
 *
 * @note
 * This function does not modify the position.
 */
bool ChessEngine::isSquareAttacked(const ChessPosition &p,
                                   int row,
                                   int col,
                                   Color attacker)
{
    return isSquareAttackedIn(p.board, row, col, attacker);
}

/**
 * @brief Checks whether the given color is currently in check.
 *
 * @param p
 * The position to inspect.
 *
 * @param color
 * The color whose king may be in check.
 *
 * @return
 * true if the king of the given color is attacked by the opponent,
 * otherwise false.
 *
 * @details
 * This function:
 * 1. Finds the king of the given color.
 * 2. Checks whether that square is attacked by the opposite color.
 *
 * @note
 * If the king cannot be found, this function returns false.
 *
 * @note
 * This function does not modify the position.
 */
bool ChessEngine::isInCheck(const ChessPosition &p, Color color)
{
    int kingRow = -1;
    int kingCol = -1;

    if (!findKing(p, color, kingRow, kingCol))
        return false;

    return isSquareAttacked(p, kingRow, kingCol, opponent(color));
}


/**
 * @brief Appends straight-line moves in one direction for rook, bishop, or queen moves.
 *
 * @param p
 * The position containing the moving piece.
 *
 * @param row
 * The row of the moving piece.
 *
 * @param col
 * The column of the moving piece.
 *
 * @param dr
 * Row step: -1, 0, or +1.
 *
 * @param dc
 * Column step: -1, 0, or +1.
 *
 * @param color
 * The color of the moving piece.
 *
 * @param out
 * The move list to append to.
 *
 * @details
 * This helper generates moves along one straight line:
 * - vertically or horizontally for rooks/queens,
 * - diagonally for bishops/queens.
 *
 * It stops at the first occupied square.
 *
 * It allows:
 * - moving onto empty squares,
 * - capturing enemy non-king pieces.
 *
 * It does not allow:
 * - capturing the enemy king,
 * - jumping over pieces.
 *
 * @note
 * This generates pseudo-legal moves only.
 * It does not verify whether the move leaves the mover's own king in check.
 *
 * @note
 * This function does not modify the position.
 */
void ChessEngine::addStraightMoves(const ChessPosition &p,
                                   int row,
                                   int col,
                                   int dr,
                                   int dc,
                                   Color color,
                                   std::vector<Move> &out)
{
    int r = row + dr;
    int c = col + dc;

    while (isOnBoard(r, c))
    {
        const Piece &target = p.board[r][c];

        if (target.isEmpty())
        {
            out.push_back(Move{row, col, r, c});
        }
        else
        {
            if (target.color != color && target.type != PieceType::King)
            {
                out.push_back(Move{row, col, r, c});
            }

            break;
        }

        r += dr;
        c += dc;
    }
}

/**
 * @brief Generates pseudo-legal moves for a single piece.
 *
 * @param p
 * The position containing the moving piece.
 *
 * @param row
 * The row of the moving piece.
 *
 * @param col
 * The column of the moving piece.
 *
 * @param out
 * The move list to append to.
 *
 * @details
 * Pseudo-legal means:
 * - the move follows the basic movement rules for the piece,
 * - but the move may leave the mover's own king in check.
 *
 * This function generates:
 * - pawn forward moves,
 * - pawn double moves,
 * - pawn captures,
 * - en passant moves,
 * - knight moves,
 * - king moves,
 * - castling moves,
 * - bishop moves,
 * - rook moves,
 * - queen moves.
 *
 * @note
 * This function does not remove moves that leave the king in check.
 * Use legalMoves() for fully legal moves.
 *
 * @note
 * This function does not modify the position.
 */
void ChessEngine::generatePseudoLegalMoves(const ChessPosition &p,
                                           int row,
                                           int col,
                                           std::vector<Move> &out)
{
    const Piece &piece = p.board[row][col];

    if (piece.isEmpty())
        return;

    const Color color = piece.color;

    auto canLand = [&p, color](int r, int c) -> bool
    {
        if (!isOnBoard(r, c))
            return false;

        const Piece &target = p.board[r][c];

        // Do not allow capturing the enemy king.
        if (target.type == PieceType::King)
            return false;

        return target.isEmpty() || target.color != color;
    };

    auto addMove = [&](int r, int c)
    {
        out.push_back(Move{row, col, r, c});
    };

    switch (piece.type)
    {
        case PieceType::Pawn:
        {
            const int dir = (color == Color::White) ? -1 : 1;
            const int startRow = (color == Color::White) ? 6 : 1;

            // Forward move.
            if (isOnBoard(row + dir, col) &&
                p.board[row + dir][col].isEmpty())
            {
                addMove(row + dir, col);

                // Double pawn move.
                if (row == startRow &&
                    isOnBoard(row + 2 * dir, col) &&
                    p.board[row + 2 * dir][col].isEmpty())
                {
                    addMove(row + 2 * dir, col);
                }
            }

            // Captures and en passant.
            const int dc[2] = {-1, 1};

            for (int i = 0; i < 2; ++i)
            {
                const int r = row + dir;
                const int c = col + dc[i];

                if (!isOnBoard(r, c))
                    continue;

                const Piece &target = p.board[r][c];

                const bool normalCapture =
                !target.isEmpty() &&
                target.color != color &&
                target.type != PieceType::King;

                const bool enPassant =
                p.enPassantTarget.row == r &&
                p.enPassantTarget.col == c &&
                target.isEmpty();

                if (normalCapture || enPassant)
                    addMove(r, c);
            }

            break;
        }

        case PieceType::Knight:
        {
            static const int dr[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
            static const int dc[8] = {-1,  1, -2,  2, -2, 2, -1, 1};

            for (int i = 0; i < 8; ++i)
            {
                const int r = row + dr[i];
                const int c = col + dc[i];

                if (canLand(r, c))
                    addMove(r, c);
            }

            break;
        }

        case PieceType::King:
        {
            // Normal king moves.
            for (int dr = -1; dr <= 1; ++dr)
            {
                for (int dc = -1; dc <= 1; ++dc)
                {
                    if (dr == 0 && dc == 0)
                        continue;

                    const int r = row + dr;
                    const int c = col + dc;

                    if (canLand(r, c))
                        addMove(r, c);
                }
            }

            // Castling.
            if (color == Color::White && row == 7 && col == 4 && !isInCheck(p, color))
            {
                // White kingside: e1-g1
                if (p.canCastleWhiteKingSide &&
                    p.board[7][5].isEmpty() &&
                    p.board[7][6].isEmpty() &&
                    !isSquareAttacked(p, 7, 5, Color::Black) &&
                    !isSquareAttacked(p, 7, 6, Color::Black))
                {
                    addMove(7, 6);
                }

                // White queenside: e1-c1
                if (p.canCastleWhiteQueenSide &&
                    p.board[7][1].isEmpty() &&
                    p.board[7][2].isEmpty() &&
                    p.board[7][3].isEmpty() &&
                    !isSquareAttacked(p, 7, 2, Color::Black) &&
                    !isSquareAttacked(p, 7, 3, Color::Black))
                {
                    addMove(7, 2);
                }
            }

            if (color == Color::Black && row == 0 && col == 4 && !isInCheck(p, color))
            {
                // Black kingside: e8-g8
                if (p.canCastleBlackKingSide &&
                    p.board[0][5].isEmpty() &&
                    p.board[0][6].isEmpty() &&
                    !isSquareAttacked(p, 0, 5, Color::White) &&
                    !isSquareAttacked(p, 0, 6, Color::White))
                {
                    addMove(0, 6);
                }

                // Black queenside: e8-c8
                if (p.canCastleBlackQueenSide &&
                    p.board[0][1].isEmpty() &&
                    p.board[0][2].isEmpty() &&
                    p.board[0][3].isEmpty() &&
                    !isSquareAttacked(p, 0, 2, Color::White) &&
                    !isSquareAttacked(p, 0, 3, Color::White))
                {
                    addMove(0, 2);
                }
            }

            break;
        }

        case PieceType::Bishop:
        {
            addStraightMoves(p, row, col, -1, -1, color, out);
            addStraightMoves(p, row, col, -1,  1, color, out);
            addStraightMoves(p, row, col,  1, -1, color, out);
            addStraightMoves(p, row, col,  1,  1, color, out);

            break;
        }

        case PieceType::Rook:
        {
            addStraightMoves(p, row, col, -1,  0, color, out);
            addStraightMoves(p, row, col,  1,  0, color, out);
            addStraightMoves(p, row, col,  0, -1, color, out);
            addStraightMoves(p, row, col,  0,  1, color, out);

            break;
        }

        case PieceType::Queen:
        {
            // Diagonals.
            addStraightMoves(p, row, col, -1, -1, color, out);
            addStraightMoves(p, row, col, -1,  1, color, out);
            addStraightMoves(p, row, col,  1, -1, color, out);
            addStraightMoves(p, row, col,  1,  1, color, out);

            // Orthogonal lines.
            addStraightMoves(p, row, col, -1, 0, color, out);
            addStraightMoves(p, row, col,  1, 0, color, out);
            addStraightMoves(p, row, col,  0, -1, color, out);
            addStraightMoves(p, row, col,  0,  1, color, out);

            break;
        }

        case PieceType::None:
            break;
    }
}

// -----------------------------------------------------------------------------
// Legality testing
// -----------------------------------------------------------------------------
/**
 * @brief Checks whether a move leaves the mover's own king in check.
 *
 * @param p
 * The current position before the move is made.
 *
 * @param fromRow
 * Row of the moving piece.
 *
 * @param fromCol
 * Column of the moving piece.
 *
 * @param toRow
 * Destination row.
 *
 * @param toCol
 * Destination column.
 *
 * @return
 * true if the move leaves the mover's own king attacked, otherwise false.
 *
 * @details
 * This function simulates the move on a temporary board and checks the
 * king square.
 *
 * It handles:
 * - normal captures,
 * - en passant captures,
 * - castling rook movement,
 * - pawn promotion for legality testing.
 *
 * @note
 * This function does not modify the original position.
 *
 * @note
 * This is the core function used to filter pseudo-legal moves into
 * fully legal moves.
 */
bool ChessEngine::moveLeavesKingInCheck(const ChessPosition &p,
                                        int fromRow,
                                        int fromCol,
                                        int toRow,
                                        int toCol)
{
    Piece temp[8][8];

    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            temp[r][c] = p.board[r][c];
        }
    }

    const Piece moving = temp[fromRow][fromCol];

    const bool enPassant = isEnPassantMove(p, fromRow, fromCol, toRow, toCol, moving);
    const bool castle = isCastleMove(p, fromRow, fromCol, toRow, toCol, moving);

    // Move piece.
    temp[toRow][toCol] = moving;
    temp[fromRow][fromCol] = Piece{};

    // En passant: remove the captured pawn.
    if (enPassant)
    {
        const int capturedRow = (moving.color == Color::White) ? toRow + 1 : toRow - 1;

        if (isOnBoard(capturedRow, toCol))
        {
            temp[capturedRow][toCol] = Piece{};
        }
    }

    // Castling: move the rook.
    if (castle)
    {
        if (moving.color == Color::White)
        {
            if (toCol == 6)
            {
                temp[7][5] = temp[7][7];
                temp[7][7] = Piece{};
            }
            else
            {
                temp[7][3] = temp[7][0];
                temp[7][0] = Piece{};
            }
        }
        else
        {
            if (toCol == 6)
            {
                temp[0][5] = temp[0][7];
                temp[0][7] = Piece{};
            }
            else
            {
                temp[0][3] = temp[0][0];
                temp[0][0] = Piece{};
            }
        }
    }

    // Promotion for legality testing.
    if (moving.type == PieceType::Pawn &&
        ((moving.color == Color::White && toRow == 0) ||
        (moving.color == Color::Black && toRow == 7)))
    {
        temp[toRow][toCol] = {moving.color, PieceType::Queen};
    }

    const Color myColor = moving.color;

    int kingRow = -1;
    int kingCol = -1;

    if (!findKingIn(temp, myColor, kingRow, kingCol))
        return false;

    return isSquareAttackedIn(temp, kingRow, kingCol, opponent(myColor));
}

/**
 * @brief Returns fully legal moves for a single piece.
 *
 * @param p
 * The position to inspect.
 *
 * @param row
 * The row of the moving piece.
 *
 * @param col
 * The column of the moving piece.
 *
 * @return
 * A vector of legal destination moves for the piece.
 *
 * @details
 * This function:
 * 1. Generates pseudo-legal moves.
 * 2. Removes any move that leaves the mover's own king in check.
 *
 * @note
 * This function does not modify the position.
 *
 * @note
 * This may be relatively expensive because it simulates each candidate move.
 */
std::vector<ChessEngine::Move> ChessEngine::legalMoves(const ChessPosition &p,
                                                       int row,
                                                       int col)
{
    std::vector<Move> pseudoMoves;
    generatePseudoLegalMoves(p, row, col, pseudoMoves);

    std::vector<Move> legal;

    for (const Move &m : pseudoMoves)
    {
        if (!moveLeavesKingInCheck(p, m.fromRow, m.fromCol, m.toRow, m.toCol))
        {
            legal.push_back(m);
        }
    }

    return legal;
}

/**
 * @brief Returns all fully legal moves for the given side.
 *
 * @param p
 * The position to inspect.
 *
 * @param color
 * The side whose moves are being generated.
 *
 * @return
 * A vector containing all legal moves for the given color.
 *
 * @details
 * This function loops over every piece owned by the given color and
 * collects the legal moves for each piece.
 *
 * @note
 * This function does not modify the position.
 *
 * @note
 * This is used by:
 * - hasAnyLegalMove(),
 * - minimax(),
 * - findBestMove(),
 * - and UI code that needs legal move targets.
 */
std::vector<ChessEngine::Move> ChessEngine::legalMovesForColor(const ChessPosition &p,
                                                               Color color)
{
    std::vector<Move> moves;

    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            if (p.board[r][c].color == color && !p.board[r][c].isEmpty())
            {
                std::vector<Move> pieceMoves = legalMoves(p, r, c);
                moves.insert(moves.end(), pieceMoves.begin(), pieceMoves.end());
            }
        }
    }

    return moves;
}

/**
 * @brief Checks whether the given side has at least one legal move.
 *
 * @param p
 * The position to inspect.
 *
 * @param color
 * The side to test.
 *
 * @return
 * true if the side has at least one legal move, otherwise false.
 *
 * @details
 * This is used to detect:
 * - checkmate: side has no legal moves and is in check,
 * - stalemate: side has no legal moves and is not in check.
 *
 * @note
 * This function does not modify the position.
 */
bool ChessEngine::hasAnyLegalMove(const ChessPosition &p, Color color)
{
    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            if (p.board[r][c].color == color && !p.board[r][c].isEmpty())
            {
                if (!legalMoves(p, r, c).empty())
                    return true;
            }
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// Evaluation
// -----------------------------------------------------------------------------

/**
 * @brief Returns the material value of a piece type.
 *
 * @param t
 * The piece type.
 *
 * @return
 * A numeric material value.
 *
 * @details
 * Current values are simple material values:
 * - pawn,
 * - knight,
 * - bishop,
 * - rook,
 * - queen,
 * - king.
 *
 * @note
 * The king value is very large because it is not intended to be captured
 * in legal play.
 *
 * @note
 * These values are used by evaluate() and move ordering.
 */
int ChessEngine::pieceValue(PieceType t)
{
    switch (t)
    {
        case PieceType::Pawn:
            return 100;

        case PieceType::Knight:
            return 300;

        case PieceType::Bishop:
            return 320;

        case PieceType::Rook:
            return 500;

        case PieceType::Queen:
            return 900;

        case PieceType::King:
            return 100000;

        default:
            return 0;
    }
}

/**
 * @brief Evaluates the current position from White's point of view.
 *
 * @param p
 * The position to evaluate.
 *
 * @return
 * A numeric score.
 *
 * @details
 * Positive values favor White.
 * Negative values favor Black.
 *
 * The current evaluation is simple:
 * - material balance,
 * - small center-control bonus.
 *
 * @note
 * This is a leaf-node evaluation used by minimax when depth reaches zero.
 *
 * @note
 * This function does not modify the position.
 *
 * @todo
 * Later improvements can add advanced features:
 * - piece-square tables,
 * - mobility,
 * - king safety,
 * - pawn structure,
 * - bishop pair,
 * - rook on open files,
 * - development.
 */
int ChessEngine::evaluate(const ChessPosition &p)
{
    int score = 0;

    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            const Piece &piece = p.board[r][c];

            if (piece.isEmpty())
                continue;

            int value = pieceValue(piece.type);
            value += piecePositionBonus(piece.type, r, c, piece.color);

            if (piece.color == Color::White)
                score += value;
            else
                score -= value;
        }
    }

    return score;
}



// -----------------------------------------------------------------------------
// Special move helpers
// -----------------------------------------------------------------------------

/**
 * @brief Checks whether a move is an en passant capture.
 *
 * @param p
 * The position before the move.
 *
 * @param fromRow
 * Row of the moving pawn.
 *
 * @param fromCol
 * Column of the moving pawn.
 *
 * @param toRow
 * Destination row.
 *
 * @param toCol
 * Destination column.
 *
 * @param moving
 * The moving piece.
 *
 * @return
 * true if the move is an en passant capture, otherwise false.
 *
 * @details
 * An en passant move is recognized when:
 * - the moving piece is a pawn,
 * - the destination square is empty,
 * - the destination square matches the current en-passant target,
 * - the move is diagonal.
 *
 * @note
 * This function does not modify the position.
 */
bool ChessEngine::isEnPassantMove(const ChessPosition &p,
                                  int fromRow,
                                  int fromCol,
                                  int toRow,
                                  int toCol,
                                  const Piece &moving)
{
    return moving.type == PieceType::Pawn &&
    p.board[toRow][toCol].isEmpty() &&
    p.enPassantTarget.row == toRow &&
    p.enPassantTarget.col == toCol &&
    toRow != fromRow &&
    toCol != fromCol;
}

/**
 * @brief Checks whether a move is a castling move.
 *
 * @param p
 * The position before the move.
 *
 * @param fromRow
 * Row of the moving king.
 *
 * @param fromCol
 * Column of the moving king.
 *
 * @param toRow
 * Destination row.
 *
 * @param toCol
 * Destination column.
 *
 * @param moving
 * The moving piece.
 *
 * @return
 * true if the move is a valid castling move according to castling rights,
 * otherwise false.
 *
 * @details
 * This function checks:
 * - the moving piece is a king,
 * - the king is on e1 or e8,
 * - the destination is the castling destination,
 * - the appropriate castling right exists.
 *
 * It does not re-check:
 * - squares between king and rook,
 * - whether the king passes through check,
 * - whether the king is currently in check.
 *
 * Those checks are performed during move generation.
 *
 * @note
 * This function does not modify the position.
 */
bool ChessEngine::isCastleMove(const ChessPosition &p,
                               int fromRow,
                               int fromCol,
                               int toRow,
                               int toCol,
                               const Piece &moving)
{
    if (moving.type != PieceType::King)
        return false;

    // King must be on e1/e8.
    if (fromCol != 4)
        return false;

    if (fromRow != 7 && fromRow != 0)
        return false;

    // King destination must be on the same rank.
    if (toRow != fromRow)
        return false;

    // Kingside castling: e1-g1 or e8-g8
    if (toCol == 6)
    {
        if (fromRow == 7)
            return p.canCastleWhiteKingSide;

        if (fromRow == 0)
            return p.canCastleBlackKingSide;
    }

    // Queenside castling: e1-c1 or e8-c8
    if (toCol == 2)
    {
        if (fromRow == 7)
            return p.canCastleWhiteQueenSide;

        if (fromRow == 0)
            return p.canCastleBlackQueenSide;
    }

    return false;
}


/**
 * @brief Updates castling rights after a move has been applied.
 *
 * @param p
 * The position being updated.
 *
 * @param fromRow
 * Row of the moving piece.
 *
 * @param fromCol
 * Column of the moving piece.
 *
 * @param toRow
 * Destination row.
 *
 * @param toCol
 * Destination column.
 *
 * @param moving
 * The piece that moved.
 *
 * @param capturedRow
 * Row of the captured piece, or -1 if no piece was captured.
 *
 * @param capturedCol
 * Column of the captured piece, or -1 if no piece was captured.
 *
 * @param captured
 * The captured piece.
 *
 * @param castle
 * true if the move was a castling move.
 *
 * @details
 * Castling rights are removed when:
 * - the king moves,
 * - a rook moves from its original castling square,
 * - a rook is captured on its original castling square,
 * - castling occurs.
 *
 * @note
 * This function modifies the position's castling rights.
 */
void ChessEngine::updateCastlingRightsAfterMove(
    ChessPosition &p,
    int fromRow,
    int fromCol,
    int toRow,
    int toCol,
    const Piece &moving,
    int capturedRow,
    int capturedCol,
    const Piece &captured,
    bool castle)
{
    // 1. King moved: that side loses all castling rights.
    if (moving.type == PieceType::King)
    {
        if (moving.color == Color::White)
        {
            p.canCastleWhiteKingSide = false;
            p.canCastleWhiteQueenSide = false;
        }
        else
        {
            p.canCastleBlackKingSide = false;
            p.canCastleBlackQueenSide = false;
        }
    }

    // 2. Rook moved from its original castling square.
    if (moving.type == PieceType::Rook)
    {
        if (moving.color == Color::White)
        {
            if (fromRow == 7 && fromCol == 0)
                p.canCastleWhiteQueenSide = false;

            if (fromRow == 7 && fromCol == 7)
                p.canCastleWhiteKingSide = false;
        }
        else
        {
            if (fromRow == 0 && fromCol == 0)
                p.canCastleBlackQueenSide = false;

            if (fromRow == 0 && fromCol == 7)
                p.canCastleBlackKingSide = false;
        }
    }

    // 3. A rook was captured.
    //
    // This must work regardless of which side made the capture.
    if (isOnBoard(capturedRow, capturedCol) &&
        !captured.isEmpty() &&
        captured.type == PieceType::Rook)
    {
        if (captured.color == Color::White)
        {
            if (capturedRow == 7 && capturedCol == 0)
                p.canCastleWhiteQueenSide = false;

            if (capturedRow == 7 && capturedCol == 7)
                p.canCastleWhiteKingSide = false;
        }
        else
        {
            if (capturedRow == 0 && capturedCol == 0)
                p.canCastleBlackQueenSide = false;

            if (capturedRow == 0 && capturedCol == 7)
                p.canCastleBlackKingSide = false;
        }
    }

    // 4. Castling itself removes both rights for the castling side.
    if (castle)
    {
        if (moving.color == Color::White)
        {
            p.canCastleWhiteKingSide = false;
            p.canCastleWhiteQueenSide = false;
        }
        else
        {
            p.canCastleBlackKingSide = false;
            p.canCastleBlackQueenSide = false;
        }
    }
}


/**
 * @brief Applies a move to the position and returns data needed to undo it.
 *
 * @param p
 * The position to modify.
 *
 * @param fromRow
 * Row of the moving piece.
 *
 * @param fromCol
 * Column of the moving piece.
 *
 * @param toRow
 * Destination row.
 *
 * @param toCol
 * Destination column.
 *
 * @return
 * A SavedMove structure containing enough state to restore the previous position.
 *
 * @details
 * This function performs the actual board mutation.
 *
 * It handles:
 * - normal moves,
 * - captures,
 * - en passant captures,
 * - castling rook movement,
 * - pawn promotion,
 * - en-passant target updates,
 * - castling right updates.
 *
 * It does not:
 * - change sideToMove,
 * - update move history,
 * - update repetition counters,
 * - update game result.
 *
 * @note
 * The caller is responsible for switching the side to move after applying
 * the move.
 *
 * @note
 * The returned SavedMove must be passed to undoMove() if the move is to be reversed.
 */
ChessEngine::SavedMove ChessEngine::applyMove(ChessPosition &p,
                                              int fromRow,
                                              int fromCol,
                                              int toRow,
                                              int toCol)
{
    SavedMove s;

    s.from = p.board[fromRow][fromCol];
    s.to = p.board[toRow][toCol];
    s.captured = s.to;
    s.promotion = PieceType::None;

    s.enPassant = isEnPassantMove(p, fromRow, fromCol, toRow, toCol, s.from);
    s.castle = isCastleMove(p, fromRow, fromCol, toRow, toCol, s.from);

    s.capturedRow = -1;
    s.capturedCol = -1;

    if (!s.to.isEmpty())
    {
        s.capturedRow = toRow;
        s.capturedCol = toCol;
    }


    // Save current castling rights.
    s.canCastleWhiteKingSide = p.canCastleWhiteKingSide;
    s.canCastleWhiteQueenSide = p.canCastleWhiteQueenSide;
    s.canCastleBlackKingSide = p.canCastleBlackKingSide;
    s.canCastleBlackQueenSide = p.canCastleBlackQueenSide;

    // Save current en passant target.
    s.enPassantTarget = p.enPassantTarget;

    // En passant: find the pawn that is actually captured.
    if (s.enPassant)
    {
        const int capturedRow = (s.from.color == Color::White) ? toRow + 1 : toRow - 1;

        if (isOnBoard(capturedRow, toCol))
        {
            s.capturedRow = capturedRow;
            s.capturedCol = toCol;
            s.captured = p.board[capturedRow][toCol];
        }
    }

    // Move the piece.
    p.board[toRow][toCol] = s.from;
    p.board[fromRow][fromCol] = Piece{};

    // Remove en passant captured pawn.
    if (s.enPassant && s.capturedRow >= 0)
    {
        p.board[s.capturedRow][s.capturedCol] = Piece{};
    }

    // Move rook for castling.
    if (s.castle)
    {
        if (s.from.color == Color::White)
        {
            if (toCol == 6)
            {
                // O-O
                s.rookFromRow = 7;
                s.rookFromCol = 7;
                s.rookToRow = 7;
                s.rookToCol = 5;
            }
            else
            {
                // O-O-O
                s.rookFromRow = 7;
                s.rookFromCol = 0;
                s.rookToRow = 7;
                s.rookToCol = 3;
            }
        }
        else
        {
            if (toCol == 6)
            {
                // O-O
                s.rookFromRow = 0;
                s.rookFromCol = 7;
                s.rookToRow = 0;
                s.rookToCol = 5;
            }
            else
            {
                // O-O-O
                s.rookFromRow = 0;
                s.rookFromCol = 0;
                s.rookToRow = 0;
                s.rookToCol = 3;
            }
        }

        s.rook = p.board[s.rookFromRow][s.rookFromCol];

        p.board[s.rookToRow][s.rookToCol] = s.rook;
        p.board[s.rookFromRow][s.rookFromCol] = Piece{};
    }

    // Pawn promotion.
    if (s.from.type == PieceType::Pawn &&
        ((s.from.color == Color::White && toRow == 0) ||
        (s.from.color == Color::Black && toRow == 7)))
    {
        s.promotion = PieceType::Queen;
        p.board[toRow][toCol] = {s.from.color, PieceType::Queen};
    }

    // Update en passant target.
    p.enPassantTarget = Square{-1, -1};
    if (s.from.type == PieceType::Pawn && std::abs(toRow - fromRow) == 2)
    {
        p.enPassantTarget = Square{(fromRow + toRow) / 2, toCol};
    }

    // Update castling rights.
    updateCastlingRightsAfterMove(
        p,
        fromRow,
        fromCol,
        toRow,
        toCol,
        s.from,
        s.capturedRow,
        s.capturedCol,
        s.captured,
        s.castle
    );

    return s;
}

/**
 * @brief Restores a position from a previously saved move.
 *
 * @param p
 * The position to restore.
 *
 * @param saved
 * The SavedMove returned by applyMove().
 *
 * @param fromRow
 * Row of the move being undone.
 *
 * @param fromCol
 * Column of the move being undone.
 *
 * @param toRow
 * Destination row of the move being undone.
 *
 * @param toCol
 * Destination column of the move being undone.
 *
 * @details
 * This function reverses:
 * - the moving piece,
 * - captured pieces,
 * - en passant captures,
 * - castling rook movement,
 * - castling rights,
 * - en-passant target.
 *
 * It does not:
 * - change sideToMove,
 * - update move history,
 * - update repetition counters,
 * - update game result.
 *
 * @note
 * The caller is responsible for restoring the previous side to move.
 *
 * @note
 * The from/to coordinates should match the move that produced the SavedMove.
 */
void ChessEngine::undoMove(ChessPosition &p,
                           const SavedMove &s,
                           int fromRow,
                           int fromCol,
                           int toRow,
                           int toCol)
{
    p.board[fromRow][fromCol] = s.from;
    p.board[toRow][toCol] = s.to;

    if (s.enPassant && s.capturedRow >= 0)
    {
        p.board[s.capturedRow][s.capturedCol] = s.captured;
    }

    if (s.castle)
    {
        p.board[s.rookFromRow][s.rookFromCol] = s.rook;
        p.board[s.rookToRow][s.rookToCol] = Piece{};
    }

    p.canCastleWhiteKingSide = s.canCastleWhiteKingSide;
    p.canCastleWhiteQueenSide = s.canCastleWhiteQueenSide;
    p.canCastleBlackKingSide = s.canCastleBlackKingSide;
    p.canCastleBlackQueenSide = s.canCastleBlackQueenSide;

    p.enPassantTarget = s.enPassantTarget;
}

// -----------------------------------------------------------------------------
// Search
// -----------------------------------------------------------------------------


/**
 * @brief Scores a move for move ordering.
 *
 * @param p
 * The position before the move is made.
 *
 * @param m
 * The move to score.
 *
 * @return
 * A heuristic score used to order moves.
 *
 * @details
 * Higher scores should be searched first.
 *
 * The current ordering prefers:
 * - captures,
 * - especially captures of valuable pieces,
 * - promotions.
 *
 * @note
 * This is only a heuristic. It does not affect correctness, only speed.
 *
 * @note
 * Good move ordering improves alpha-beta pruning.
 */
int ChessEngine::moveScore(const ChessPosition &p, const Move &m)
{
    int score = 0;

    const Piece &target = p.board[m.toRow][m.toCol];
    const Piece &moving = p.board[m.fromRow][m.fromCol];

    if (!target.isEmpty())
    {
        score += 10 * pieceValue(target.type) - pieceValue(moving.type);
    }

    if (moving.type == PieceType::Pawn &&
        ((moving.color == Color::White && m.toRow == 0) ||
        (moving.color == Color::Black && m.toRow == 7)))
    {
        score += 1000;
    }

    return score;
}

/**
 * @brief Recursively searches the game tree using minimax with alpha-beta pruning.
 *
 * @param p
 * The position being searched. This is modified temporarily by applyMove()
 * and undoMove() during the search.
 *
 * @param toMove
 * The side that is to move at this node.
 *
 * @param depth
 * Remaining search depth.
 *
 * @param alpha
 * Best score the maximizing player is guaranteed so far.
 *
 * @param beta
 * Best score the minimizing player is guaranteed so far.
 *
 * @return
 * The evaluated score for the position from White's point of view.
 *
 * @details
 * This is the core search function.
 *
 * It:
 * 1. Generates legal moves.
 * 2. Handles checkmate/stalemate terminal nodes.
 * 3. Returns evaluate() at leaf nodes.
 * 4. Orders moves heuristically.
 * 5. Applies each move.
 * 6. Recurses to the next depth.
 * 7. Undoes the move.
 * 8. Updates alpha/beta pruning bounds.
 *
 * @note
 * This function is not thread-safe if multiple threads search the same
 * ChessPosition object.
 *
 * @note
 * The current design avoids that by giving the worker thread its own copy
 * of the position.
 *
 * @note
 * Checkmate scores are adjusted by depth so that faster mates are preferred.
 */
int ChessEngine::minimax(ChessPosition &p,
                         Color toMove,
                         int depth,
                         int alpha,
                         int beta)
{
    const int INF = 1000000000;

    std::vector<Move> moves = legalMovesForColor(p, toMove);

    // Terminal position.
    if (moves.empty())
    {
        if (isInCheck(p, toMove))
        {
            return (toMove == Color::White)
            ? -INF - depth
            : INF + depth;
        }

        // Stalemate.
        return 0;
    }

    // Leaf node.
    if (depth <= 0)
        return evaluate(p);

    // Move ordering: captures/promotions first.
    std::sort(moves.begin(),
              moves.end(),
              [&p](const Move &a, const Move &b)
              {
                  return moveScore(p, a) > moveScore(p, b);
              });

    if (toMove == Color::White)
    {
        int best = -INF;

        for (const Move &m : moves)
        {
            SavedMove saved = applyMove(p, m.fromRow, m.fromCol, m.toRow, m.toCol);

            int score = minimax(p, Color::Black, depth - 1, alpha, beta);

            undoMove(p, saved, m.fromRow, m.fromCol, m.toRow, m.toCol);

            best = std::max(best, score);
            alpha = std::max(alpha, best);

            if (alpha >= beta)
                break;
        }

        return best;
    }
    else
    {
        int best = INF;

        for (const Move &m : moves)
        {
            SavedMove saved = applyMove(p, m.fromRow, m.fromCol, m.toRow, m.toCol);

            int score = minimax(p, Color::White, depth - 1, alpha, beta);

            undoMove(p, saved, m.fromRow, m.fromCol, m.toRow, m.toCol);

            best = std::min(best, score);
            beta = std::min(beta, best);

            if (alpha >= beta)
                break;
        }

        return best;
    }
}

/**
 * @brief Finds the best move for the given color at the given depth.
 *
 * @param start
 * The position to search from.
 *
 * @param aiColor
 * The color the search is trying to play best.
 *
 * @param depth
 * Search depth in plies.
 *
 * @return
 * The best move found by the search.
 *
 * @details
 * This is the main entry point used by the AI worker thread.
 *
 * It:
 * 1. Makes a local search copy of the starting position.
 * 2. Generates legal moves for the AI color.
 * 3. Searches each root move using minimax().
 * 4. Tracks the best root move and score.
 * 5. Returns the best move.
 *
 * @note
 * This function is designed to be called from a worker thread on a local
 * copy of the position.
 *
 * @note
 * It should not be called with the live GUI position if that position may
 * be modified at the same time.
 *
 * @note
 * The current implementation does not use a time limit.
 * A future improvement could add iterative deepening with a maximum search time.
 */
ChessEngine::Move ChessEngine::findBestMove(const ChessPosition &start,
                                            Color aiColor,
                                            int depth)
{
    if (depth < 1)
        depth = 1;

    if (depth > 6)
        depth = 6;

    // Search on a local copy.
    ChessPosition p = start;

    std::vector<Move> moves = legalMovesForColor(p, aiColor);

    if (moves.empty())
        return Move{};

    const int INF = 1000000000;

    Move bestMove = moves.front();
    int bestScore = (aiColor == Color::White) ? -INF : INF;

    int alpha = -INF;
    int beta = INF;

    for (const Move &m : moves)
    {
        SavedMove saved = applyMove(p, m.fromRow, m.fromCol, m.toRow, m.toCol);

        int score = minimax(p, opponent(aiColor), depth - 1, alpha, beta);

        undoMove(p, saved, m.fromRow, m.fromCol, m.toRow, m.toCol);

        if (aiColor == Color::White)
        {
            if (score > bestScore)
            {
                bestScore = score;
                bestMove = m;
            }

            alpha = std::max(alpha, bestScore);
        }
        else
        {
            if (score < bestScore)
            {
                bestScore = score;
                bestMove = m;
            }

            beta = std::min(beta, bestScore);
        }

        if (alpha >= beta)
            break;
    }

    return bestMove;
}

/**
 * @brief Check for insufficent pieces  on board for a draw.
 *
 * @note simple implementation of insufficient-material rule
 *
 */
bool ChessEngine::isInsufficientMaterial(const ChessPosition &p)
{
    int nonKingPieces = 0;

    int whiteMinors = 0;
    int blackMinors = 0;

    int whiteBishops = 0;
    int blackBishops = 0;

    int bishopColorMask = 0;

    bool hasPawnOrMajor = false;

    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            const Piece &piece = p.board[r][c];

            if (piece.isEmpty())
                continue;

            if (piece.type == PieceType::King)
                continue;

            nonKingPieces++;

            if (piece.type == PieceType::Pawn ||
                piece.type == PieceType::Rook ||
                piece.type == PieceType::Queen)
            {
                hasPawnOrMajor = true;
            }

            if (piece.type == PieceType::Knight ||
                piece.type == PieceType::Bishop)
            {
                if (piece.color == Color::White)
                    whiteMinors++;
                else
                    blackMinors++;

                if (piece.type == PieceType::Bishop)
                {
                    if (piece.color == Color::White)
                        whiteBishops++;
                    else
                        blackBishops++;

                    int squareColor = (r + c) % 2;
                    bishopColorMask |= (1 << squareColor);
                }
            }
        }
    }

    // Any pawn, rook, or queen means material is sufficient for potential mate.
    if (hasPawnOrMajor)
        return false;

    // Kings only.
    if (nonKingPieces == 0)
        return true;

    // King + one minor versus King.
    if ((whiteMinors == 1 && blackMinors == 0) ||
        (whiteMinors == 0 && blackMinors == 1))
    {
        return true;
    }

    // Bishops only, and all bishops on the same square color.
    if (whiteMinors == whiteBishops &&
        blackMinors == blackBishops &&
        whiteBishops > 0 &&
        blackBishops > 0)
    {
        if (bishopColorMask == 1 || bishopColorMask == 2)
            return true;
    }

    return false;
}

