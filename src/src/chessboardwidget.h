/*
 * Copyright 2026 Alan Crispin <crispinalan@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef CHESSBOARDWIDGET_H
#define CHESSBOARDWIDGET_H

#include <QWidget>
#include <QPoint>
#include <QString>
#include <QRect>
#include <QPainter>
#include <QPaintEvent>
#include <QColor>
#include <QMouseEvent>
#include <QKeyEvent>

#include <functional>
#include <future>
#include <thread>
#include <vector>

#include "chessposition.h"
#include "chessengine.h"

#include <map>
#include <string>


class ChessBoardWidget : public QWidget
{
public:
    explicit ChessBoardWidget(QWidget *parent = nullptr);
    ~ChessBoardWidget() override;

    using Color = ChessPosition::Color;
    using PieceType = ChessPosition::PieceType;
    using Piece = ChessPosition::Piece;
    using Move = ChessPosition::Move;

    /**
     * @brief Starts a new game.
     */
    void newGame();

    /**
     * @brief Returns human-readable game status.
     */
    QString statusText() const;

    /**
     * @brief True when the game has ended by checkmate or stalemate.
     */
    bool isGameOver() const;

    /**
     * @brief Optional callback for updating MainWindow status label.
     */
    void setStatusCallback(std::function<void(const QString &)> callback);

    // AI controls
    void setAiEnabled(bool enabled);
    void setAiDepth(int depth);
    void setAiPlaysWhite(bool playsWhite);
    void startAiMove();

    QString moveHistoryText() const;
    void printMoveHistory() const;

    QString lastMoveText() const;
    QString moveHistoryTailText(int maxFullMoves) const;
    std::size_t moveHistorySize() const;
    void undoLastTurn();



    void setRepetitionDrawEnabled(bool enabled);
    void setFiftyMoveDrawEnabled(bool enabled);
    void setInsufficientMaterialDrawEnabled(bool enabled);

    bool repetitionDrawEnabledValue() const;
    bool fiftyMoveDrawEnabledValue() const;
    bool insufficientMaterialDrawEnabledValue() const;

    //White best next move (hint feature)
    bool hintThinking = false;

    std::thread hintThread;
    std::future<Move> hintFuture;

    int hintRequestId = 0;
    int hintActiveRequestId = 0;

    QString lastHint;

    void requestHint();
    void pollHintResult();
    QString hintText() const;

private:

    //GameResult contains user selectable draw rules
    enum class GameResult
    {
        Ongoing,
        Check,
        Checkmate,
        Stalemate,
        Repetition,
        FiftyMove,
        InsufficientMaterial
    };


    struct MoveRecord
    {
        int fromRow = 0;
        int fromCol = 0;
        int toRow = 0;
        int toCol = 0;

        Piece captured;
        PieceType promotion = PieceType::None;

        bool enPassant = false;
        bool castle = false;
        bool aiMove = false;

        int halfMoveClockBeforeMove = 0;
    };



    // -------------------------------------------------------------------------
    // Game state
    // -------------------------------------------------------------------------

    void resetGame();
    void updateGameStatus();

    void makeMove(int fromRow, int fromCol, int toRow, int toCol);
    //void executeMove(int fromRow, int fromCol, int toRow, int toCol);
    void executeMove(int fromRow, int fromCol, int toRow, int toCol, bool aiMove);
    QString moveRecordText(const MoveRecord &m) const;


    void selectPiece(int row, int col);
    void deselect();

    QString squareName(int row, int col) const;

    //------------------------------------------------------------------------
    // undo
    //---------------------------------------------------------------------------

    //Each executed move stores the full ChessEngine::SavedMove,
    //which contains enough information to reverse the move.

    std::vector<ChessEngine::SavedMove> undoStack;


    // -------------------------------------------------------------------------
    // AI
    // -------------------------------------------------------------------------

    void maybeScheduleAiMove();
    void pollAiResult();

    // -------------------------------------------------------------------------
    // Drawing
    // -------------------------------------------------------------------------

    int cellSize() const;
    QRect squareRect(int row, int col) const;

    void drawPawn(QPainter &p, const QRect &rect, const Piece &piece);
    void drawRook(QPainter &p, const QRect &rect, const Piece &piece);
    void drawKnight(QPainter &p, const QRect &rect, const Piece &piece);
    void drawBishop(QPainter &p, const QRect &rect, const Piece &piece);
    void drawQueen(QPainter &p, const QRect &rect, const Piece &piece);
    void drawKing(QPainter &p, const QRect &rect, const Piece &piece);

    void drawBoard(QPainter &painter);
    void drawPieces(QPainter &painter);
    void drawSelection(QPainter &painter);
    void drawPiece(QPainter &painter, const QRect &rect, const Piece &piece);

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    ChessPosition position;

    int selectedRow = -1;
    int selectedCol = -1;

    std::vector<Move> legalTargets;

    bool gameOver = false;
    GameResult gameResult = GameResult::Ongoing;
    Color winner = Color::White;

    std::function<void(const QString &)> statusCallback;

    std::vector<MoveRecord> moveHistory;

    //repetition moves
    std::map<std::string, int> positionCounts;
    std::vector<std::string> positionKeyHistory;
    bool isCurrentPositionRepeatedThreeTimes() const;

    bool repetitionDrawEnabled = false;
    bool fiftyMoveDrawEnabled = false;
    bool insufficientMaterialDrawEnabled = false;

    int halfMoveClock = 0;




    // AI state
    bool aiThinking = false;
    bool aiEnabled = false;
    Color aiColor = Color::Black;
    int aiDepth = 2;

    // Thread/future state
    std::thread aiThread;
    std::future<Move> aiFuture;

    // Used to discard stale AI searches after New Game or other state changes.
    int aiRequestId = 0;
    int aiActiveRequestId = 0;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // CHESSBOARDWIDGET_H
