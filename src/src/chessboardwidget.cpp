/*
 * Copyright 2026 Alan Crispin <crispinalan@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "chessboardwidget.h"

#include <QPainter>
#include <QPen>
#include <QColor>
#include <QRect>
#include <QFont>
#include <QTimer>
#include <QDebug>
#include <QPaintEvent>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>

#include <chrono>
#include <utility>

// -----------------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------------

/**
 * @brief Constructs the chess board widget.
 *
 * @details
 * This constructor:
 * - creates the widget,
 * - sets the focus policy,
 * - resets the game to the starting position.
 *
 * @note
 * The widget owns a ChessPosition object.
 *
 * @note
 * The widget does not own the chess rules directly; rules and search are
 * provided by ChessEngine.
 */
ChessBoardWidget::ChessBoardWidget(QWidget *parent)
: QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    resetGame();
}

/**
 * @brief Destroys the chess board widget.
 *
 * @details
 * If an AI worker thread is still running or joinable, this destructor
 * joins it before the widget is destroyed.
 *
 * @note
 * This may block the GUI thread until the AI search finishes.
 *
 * @note
 * This is important because a joinable std::thread must not be destroyed
 * directly.
 */
ChessBoardWidget::~ChessBoardWidget()
{
    if (aiThread.joinable())
        aiThread.join();

    if (hintThread.joinable())
        hintThread.join();
}


// -----------------------------------------------------------------------------
// Game setup
// -----------------------------------------------------------------------------

/**
 * @brief Resets the widget's game state to a new starting position.
 *
 * @details
 * This resets:
 * - the board position,
 * - selected piece,
 * - legal targets,
 * - game over state,
 * - move history,
 * - undo stack,
 * - repetition counters,
 * - active AI request.
 *
 * @note
 * This does not automatically start the AI.
 * Callers such as newGame() decide whether to schedule an AI move.
 *
 * @note
 * Incrementing aiRequestId invalidates any AI search that is still running
 * from the previous game.
 */
void ChessBoardWidget::resetGame()
{
    position.resetToStart();

    selectedRow = -1;
    selectedCol = -1;
    halfMoveClock = 0;

    legalTargets.clear();

    gameOver = false;
    gameResult = GameResult::Ongoing;
    winner = Color::White;

    moveHistory.clear();
    undoStack.clear();

    positionCounts.clear();
    positionKeyHistory.clear();

    const std::string startKey = position.key();
    positionCounts[startKey] = 1;

    lastHint.clear();
    hintThinking = false;


    ++aiRequestId;

    update();
}

/**
 * @brief Starts a new game.
 *
 * @details
 * This:
 * 1. Resets the board and widget state.
 * 2. Updates the game status.
 * 3. Schedules an AI move if the AI is enabled and it is the AI's turn.
 *
 * @note
 * This is the public entry point used by the MainWindow "New Game" action.
 */
void ChessBoardWidget::newGame()
{
    resetGame();
    updateGameStatus();
    maybeScheduleAiMove();
}




// -----------------------------------------------------------------------------
// Move execution
// -----------------------------------------------------------------------------

/**
 * @brief Entry point for a human-initiated move.
 *
 * @param fromRow
 * Row of the selected piece.
 *
 * @param fromCol
 * Column of the selected piece.
 *
 * @param toRow
 * Destination row.
 *
 * @param toCol
 * Destination column.
 *
 * @details
 * This marks the move as not made by the AI and forwards it to executeMove().
 *
 * @note
 * This function is called from mouse input handling.
 *
 * @note
 * It should not be called while the AI is thinking.
 */
void ChessBoardWidget::makeMove(int fromRow, int fromCol, int toRow, int toCol)
{
    executeMove(fromRow, fromCol, toRow, toCol, false);
}

/**
 * @brief Applies a move to the live game position.
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
 * @param aiMove
 * true if the move is being made by the AI, false if made by the human player.
 *
 * @details
 * This is the common move execution path used by both:
 * - human moves,
 * - AI moves.
 *
 * It:
 * 1. Applies the move using ChessEngine::applyMove().
 * 2. Stores the move in moveHistory.
 * 3. Stores the SavedMove in undoStack.
 * 4. Deselects any selected piece.
 * 5. Switches the side to move.
 * 6. Records the new position key for repetition detection.
 * 7. Updates the game status.
 * 8. Schedules an AI move if necessary.
 *
 * @note
 * This function must run on the GUI thread.
 *
 * @note
 * The AI worker thread should not call this directly.
 * It should return a Move, and the GUI thread should execute it.
 */
void ChessBoardWidget::executeMove(int fromRow,
                                   int fromCol,
                                   int toRow,
                                   int toCol,
                                   bool aiMove)
{
    if (fromRow == toRow && fromCol == toCol)
        return;

    ChessEngine::SavedMove saved =
    ChessEngine::applyMove(position, fromRow, fromCol, toRow, toCol);

    MoveRecord record;
    record.fromRow = fromRow;
    record.fromCol = fromCol;
    record.toRow = toRow;
    record.toCol = toCol;
    record.captured = saved.captured;
    record.promotion = saved.promotion;
    record.enPassant = saved.enPassant;
    record.castle = saved.castle;
    record.aiMove = aiMove;

    moveHistory.push_back(record);
    undoStack.push_back(saved);

    deselect();

    record.halfMoveClockBeforeMove = halfMoveClock;

    const bool resetsHalfMoveClock =
    saved.from.type == PieceType::Pawn ||
    !saved.captured.isEmpty();

    if (resetsHalfMoveClock)
        halfMoveClock = 0;
    else
        halfMoveClock = halfMoveClock + 1;


    position.sideToMove = ChessEngine::opponent(position.sideToMove);

    const std::string key = position.key();

    positionKeyHistory.push_back(key);
    positionCounts[key]++;

    updateGameStatus();

    maybeScheduleAiMove();
}

// -----------------------------------------------------------------------------
// Selection
// -----------------------------------------------------------------------------

/**
 * @brief Selects a piece and calculates its legal destination squares.
 *
 * @param row
 * Row of the piece to select.
 *
 * @param col
 * Column of the piece to select.
 *
 * @details
 * This:
 * - stores the selected square,
 * - generates legal moves for that piece,
 * - repaints the board to show the selection and legal targets.
 *
 * @note
 * Selection is only allowed when the game is not over.
 *
 * @note
 * This function is UI-only and does not modify the position.
 */
void ChessBoardWidget::selectPiece(int row, int col)
{
    if (gameOver)
        return;

    selectedRow = row;
    selectedCol = col;

    legalTargets = ChessEngine::legalMoves(position, row, col);

    update();
}

/**
 * @brief Clears the current piece selection.
 *
 * @details
 * This removes:
 * - the selected square,
 * - the list of legal target squares,
 * - and repaints the board.
 *
 * @note
 * This is called after a move is made or when the user cancels selection.
 */
void ChessBoardWidget::deselect()
{
    selectedRow = -1;
    selectedCol = -1;
    legalTargets.clear();

    update();
}

// -----------------------------------------------------------------------------
// AI controls
// -----------------------------------------------------------------------------

/**
 * @brief Enables or disables the AI opponent.
 *
 * @param enabled
 * true to enable the AI, false to disable it.
 *
 * @details
 * If the AI is enabled, this may immediately schedule an AI move if it is
 * currently the AI's turn.
 *
 * @note
 * Disabling the AI does not cancel a search that is already running.
 * The running search will still finish, but its result may be ignored
 * depending on the current game state.
 *
 * @note
 * This function should be called on the GUI thread.
 */
void ChessBoardWidget::setAiEnabled(bool enabled)
{
    aiEnabled = enabled;

    if (enabled)
        maybeScheduleAiMove();
}

/**
 * @brief Sets the AI search depth.
 *
 * @param depth
 * Desired search depth in plies.
 *
 * @details
 * The value is clamped to a safe range:
 * - minimum 1,
 * - maximum 6.
 *
 * @note
 * Higher depth is stronger but slower.
 *
 * @note
 * The actual search is performed on a worker thread, so increasing depth
 * should not block the GUI permanently, although it may keep the worker
 * thread busy longer.
 */
void ChessBoardWidget::setAiDepth(int depth)
{
    if (depth < 1)
        depth = 1;

    if (depth > 6)
        depth = 6;

    aiDepth = depth;
}

/**
 * @brief Sets which side the AI plays.
 *
 * @param playsWhite
 * true if the AI plays White, false if the AI plays Black.
 *
 * @details
 * This changes the AI color and may immediately schedule an AI move if it
 * is the AI's turn.
 *
 * @note
 * This function does not rotate the board visually.
 *
 * @note
 * If you later add board orientation, this function would be the natural
 * place to trigger that change.
 */
void ChessBoardWidget::setAiPlaysWhite(bool playsWhite)
{
    aiColor = playsWhite ? Color::White : Color::Black;

    maybeScheduleAiMove();
}

/**
 * @brief Requests that the AI make a move if it is the AI's turn.
 *
 * @details
 * This is a convenience wrapper around maybeScheduleAiMove().
 *
 * @note
 * This function does not search synchronously.
 *
 * @note
 * The actual search happens in a worker thread.
 *
 * @note
 * The GUI thread only schedules the search and later polls for the result.
 */
void ChessBoardWidget::startAiMove()
{
    maybeScheduleAiMove();
}

/**
 * @brief Schedules an AI search if all conditions allow the AI to move.
 *
 * @details
 * This is the main bridge between the GUI thread and the AI worker thread.
 *
 * It checks that:
 * - the AI is enabled,
 * - the game is not over,
 * - no AI search is already running,
 * - it is the AI's turn.
 *
 * If all checks pass, it:
 * 1. Sets aiThinking to true.
 * 2. Copies the live position into a local ChessPosition snapshot.
 * 3. Increments the AI request ID.
 * 4. Creates a std::promise<Move>.
 * 5. Stores the associated std::future<Move> in aiFuture.
 * 6. Joins any previously finished thread.
 * 7. Starts a new std::thread.
 * 8. Calls pollAiResult() to begin checking for completion.
 *
 * @details
 * The worker thread runs:
 *
 * ChessEngine::findBestMove(snapshot, color, depth)
 *
 * on the snapshot copy, not on the live widget position.
 *
 * This is important because the GUI thread may repaint, receive input,
 * or change the live position while the AI is searching.
 *
 * @details
 * std::promise is the writer side.
 * std::future is the reader side.
 *
 * Worker thread:
 *
 * promise.set_value(bestMove)
 *
 * GUI thread:
 *
 * aiFuture.get()
 *
 * @note
 * The lambda captures the promise by move. This transfers ownership of the
 * promise into the worker thread.
 *
 * @note
 * The lambda captures the position snapshot by move. This gives the worker
 * thread its own copy of the position.
 *
 * @note
 * The mutable lambda qualifier is required because the lambda needs to call
 * promise.set_value(), which is a non-const operation.
 *
 * @warning
 * This function may join a previous thread if one is still joinable.
 * In the current design, aiThinking prevents starting a new search while
 * the previous search is still active.
 */
void ChessBoardWidget::maybeScheduleAiMove()
{
    if (!aiEnabled)
        return;

    if (gameOver)
        return;

    if (aiThinking)
        return;

    if (position.sideToMove != aiColor)
        return;

    aiThinking = true;

    // Important: the worker searches a copy of the position, not the live widget.
    ChessPosition snapshot = position;

    const int depth = aiDepth;
    const Color color = aiColor;

    const int requestId = ++aiRequestId;
    aiActiveRequestId = requestId;

    std::promise<Move> promise;

    // Replace any old future.
    aiFuture = std::future<Move>();
    aiFuture = promise.get_future();

    // If a previous thread is still joinable, join it before starting a new one.
    if (aiThread.joinable())
        aiThread.join();

    aiThread = std::thread(
        [snapshot = std::move(snapshot), depth, color, promise = std::move(promise)]() mutable
        {
            try
            {
                Move best = ChessEngine::findBestMove(snapshot, color, depth);
                promise.set_value(best);
            }
            catch (...)
            {
                promise.set_exception(std::current_exception());
            }
        });

    pollAiResult();
}

/**
 * @brief Checks whether the AI worker thread has finished and applies the result.
 *
 * @details
 * This function is always called on the GUI thread.
 *
 * It uses std::future::wait_for() with a timeout of zero milliseconds:
 *
 * aiFuture.wait_for(std::chrono::milliseconds(0))
 *
 * This is a non-blocking readiness check.
 *
 * If the future is not ready:
 * - a QTimer schedules another call to pollAiResult(),
 * - the GUI remains responsive.
 *
 * If the future is ready:
 * 1. The worker thread is joined.
 * 2. The Move result is retrieved from the future.
 * 3. The result is checked for staleness using the request ID.
 * 4. If valid, executeMove() applies the move on the GUI thread.
 * 5. aiThinking is cleared.
 * 6. A new AI search may be scheduled if the state still requires it.
 *
 * @details
 * The request ID prevents stale AI moves.
 *
 * Example:
 * - AI starts searching.
 * - User starts a new game.
 * - AI finishes later.
 * - The old request ID no longer matches the current request ID.
 * - The old result is discarded.
 *
 * @note
 * Not using QtConcurrent but C++ futures and promises thread mechanism
 * Now got it working!
 * In C++, std::promise and std::future let one thread deliver a result (or exception) to another.
 *
 - A std::promise<T> is the producer: it sets a value or an exception.
 - A std::future<T> is the consumer’s handle to that result.
 *
 * @note
 * This function must not be called from the worker thread.
 *
 * @note
 * The worker thread only writes the Move into the promise.
 * The GUI thread reads it from the future and applies it.
 */
void ChessBoardWidget::pollAiResult()
{
    if (!aiFuture.valid())
    {
        aiThinking = false;

        if (aiEnabled && !gameOver && position.sideToMove == aiColor)
            maybeScheduleAiMove();

        return;
    }

    // Non-blocking check.
    if (aiFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        QTimer::singleShot(
            20,
            this,
            [this]()
            {
                pollAiResult();
            });

        return;
    }

    // Worker has finished, so it is safe to join.
    if (aiThread.joinable())
        aiThread.join();

    Move best;
    bool ok = true;

    try
    {
        best = aiFuture.get();
    }
    catch (...)
    {
        ok = false;
    }

    // Release the future.
    aiFuture = std::future<Move>();

    const bool stale = (aiActiveRequestId != aiRequestId);

    if (!stale && ok && !gameOver && position.sideToMove == aiColor)
    {
        executeMove(best.fromRow, best.fromCol, best.toRow, best.toCol, true);
    }
    else if (!stale && !ok)
    {
        std::vector<Move> moves = ChessEngine::legalMovesForColor(position, aiColor);

        if (!moves.empty())
        {
            executeMove(moves.front().fromRow,
                        moves.front().fromCol,
                        moves.front().toRow,
                        moves.front().toCol,
                        true);
        }
    }


    aiThinking = false;

    // If the game state is still such that the AI should move, schedule a fresh search.
    if (aiEnabled && !gameOver && position.sideToMove == aiColor)
        maybeScheduleAiMove();
}

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------

/**
 * @brief Calculates the size of one chess square.
 *
 * @return
 * The pixel size of a square.
 *
 * @details
 * The board is drawn as an 8x8 grid inside the widget.
 * The square size is based on the smaller widget dimension.
 *
 * @note
 * If the widget is too small, this returns 1 to avoid division by zero.
 */
int ChessBoardWidget::cellSize() const
{
    int side = qMin(width(), height());
    int cs = side / 8;
    return cs < 1 ? 1 : cs;
}

/**
 * @brief Returns the rectangle for a given board square.
 *
 * @param row
 * Board row, 0 to 7.
 *
 * @param col
 * Board column, 0 to 7.
 *
 * @return
 * A QRect representing the square on screen.
 *
 * @details
 * The board is centered inside the widget.
 *
 * @note
 * This function is used for:
 * - drawing pieces,
 * - drawing selection highlights,
 * - converting mouse coordinates to board squares.
 */
QRect ChessBoardWidget::squareRect(int row, int col) const
{
    int cs = cellSize();
    int boardSize = cs * 8;

    int offsetX = (width() - boardSize) / 2;
    int offsetY = (height() - boardSize) / 2;

    return QRect(offsetX + col * cs,
                 offsetY + row * cs,
                 cs,
                 cs);
}

/**
 * @brief Draws the 8x8 chessboard background.
 *
 * @param painter
 * The QPainter used for drawing.
 *
 * @details
 * This draws:
 * - the background,
 * - light squares,
 * - dark squares.
 *
 * @note
 * This function does not draw pieces or selection highlights.
 */
void ChessBoardWidget::drawBoard(QPainter &painter)
{
    int cs = cellSize();
    int boardSize = cs * 8;

    int offsetX = (width() - boardSize) / 2;
    int offsetY = (height() - boardSize) / 2;

    painter.fillRect(0, 0, width(), height(), QColor(40, 40, 40));

    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            QRect rect(offsetX + c * cs, offsetY + r * cs, cs, cs);

            if ((r + c) % 2 == 0)
                painter.fillRect(rect, QColor(240, 217, 181));
            else
                painter.fillRect(rect, QColor(181, 136, 99));
        }
    }
}

void ChessBoardWidget::drawPawn(QPainter &p, const QRect &rect, const Piece &piece)
{
    Q_UNUSED(piece);

    QPainterPath path;

    float w = rect.width();
    float h = rect.height();

    // Head
    path.addEllipse(
        rect.center().x() - w * 0.12f,
                    rect.top() + h * 0.12f,
                    w * 0.24f,
                    w * 0.24f
    );

    // Body
    QPolygonF body;
    body << QPointF(rect.center().x() - w * 0.10f, rect.top() + h * 0.38f)
    << QPointF(rect.center().x() + w * 0.10f, rect.top() + h * 0.38f)
    << QPointF(rect.center().x() + w * 0.16f, rect.bottom() - h * 0.12f)
    << QPointF(rect.center().x() - w * 0.16f, rect.bottom() - h * 0.12f);

    path.addPolygon(body);

    // Base
    path.addRoundedRect(
        rect.center().x() - w * 0.18f,
                        rect.bottom() - h * 0.14f,
                        w * 0.36f,
                        h * 0.10f,
                        w * 0.04f,
                        w * 0.04f
    );

    p.drawPath(path);
}

void ChessBoardWidget::drawRook(QPainter &p, const QRect &rect, const Piece &piece)
{
    Q_UNUSED(piece);

    QPainterPath path;

    float w = rect.width();
    float h = rect.height();

    // Body
    path.addRect(
        rect.center().x() - w * 0.18f,
                 rect.top() + h * 0.18f,
                 w * 0.36f,
                 h * 0.68f
    );

    // Battlements
    path.addRect(
        rect.center().x() - w * 0.20f,
                 rect.top() + h * 0.10f,
                 w * 0.10f,
                 h * 0.10f
    );

    path.addRect(
        rect.center().x() - w * 0.05f,
                 rect.top() + h * 0.10f,
                 w * 0.10f,
                 h * 0.10f
    );

    path.addRect(
        rect.center().x() + w * 0.10f,
                 rect.top() + h * 0.10f,
                 w * 0.10f,
                 h * 0.10f
    );

    // Base
    path.addRoundedRect(
        rect.center().x() - w * 0.22f,
                        rect.bottom() - h * 0.14f,
                        w * 0.44f,
                        h * 0.10f,
                        w * 0.04f,
                        w * 0.04f
    );

    p.drawPath(path);
}

void ChessBoardWidget::drawKnight(QPainter &p, const QRect &rect, const Piece &piece)
{
    Q_UNUSED(piece);

    QPainterPath path;

    float w = rect.width();
    float h = rect.height();

    // Base
    path.addRoundedRect(
        rect.center().x() - w * 0.20f,
                        rect.bottom() - h * 0.14f,
                        w * 0.40f,
                        h * 0.10f,
                        w * 0.04f,
                        w * 0.04f
    );

    // Neck/body
    QPolygonF neck;
    neck << QPointF(rect.center().x() - w * 0.14f, rect.bottom() - h * 0.14f)
    << QPointF(rect.center().x() + w * 0.14f, rect.bottom() - h * 0.14f)
    << QPointF(rect.center().x() + w * 0.10f, rect.top() + h * 0.34f)
    << QPointF(rect.center().x() - w * 0.10f, rect.top() + h * 0.34f);

    path.addPolygon(neck);

    // Head
    QPolygonF head;
    head << QPointF(rect.center().x() - w * 0.12f, rect.top() + h * 0.38f)
    << QPointF(rect.center().x() + w * 0.18f, rect.top() + h * 0.34f)
    << QPointF(rect.center().x() + w * 0.06f, rect.top() + h * 0.12f)
    << QPointF(rect.center().x() - w * 0.14f, rect.top() + h * 0.18f);

    path.addPolygon(head);

    // Ear
    QPolygonF ear;
    ear << QPointF(rect.center().x() - w * 0.02f, rect.top() + h * 0.14f)
    << QPointF(rect.center().x() + w * 0.06f, rect.top() + h * 0.10f)
    << QPointF(rect.center().x() - w * 0.04f, rect.top() + h * 0.04f);

    path.addPolygon(ear);

    p.drawPath(path);
}

void ChessBoardWidget::drawBishop(QPainter &p, const QRect &rect, const Piece &piece)
{
    Q_UNUSED(piece);

    QPainterPath path;

    float w = rect.width();
    float h = rect.height();

    // Base
    path.addRoundedRect(
        rect.center().x() - w * 0.18f,
                        rect.bottom() - h * 0.14f,
                        w * 0.36f,
                        h * 0.10f,
                        w * 0.04f,
                        w * 0.04f
    );

    // Body
    QPolygonF body;
    body << QPointF(rect.center().x() - w * 0.10f, rect.bottom() - h * 0.14f)
    << QPointF(rect.center().x() + w * 0.10f, rect.bottom() - h * 0.14f)
    << QPointF(rect.center().x() + w * 0.14f, rect.top() + h * 0.34f)
    << QPointF(rect.center().x() - w * 0.14f, rect.top() + h * 0.34f);

    path.addPolygon(body);

    // Mitre
    QPolygonF mitre;
    mitre << QPointF(rect.center().x(), rect.top() + h * 0.06f)
    << QPointF(rect.center().x() + w * 0.12f, rect.top() + h * 0.36f)
    << QPointF(rect.center().x() - w * 0.12f, rect.top() + h * 0.36f);

    path.addPolygon(mitre);

    p.drawPath(path);

    // Optional: small slit in the mitre
    p.setPen(QPen(QColor(0, 0, 0, 180), 2));
    p.drawLine(
        QPointF(rect.center().x(), rect.top() + h * 0.18f),
               QPointF(rect.center().x(), rect.top() + h * 0.28f)
    );
}

void ChessBoardWidget::drawQueen(QPainter &p, const QRect &rect, const Piece &piece)
{
    Q_UNUSED(piece);

    QPainterPath path;

    float w = rect.width();
    float h = rect.height();

    // Base
    path.addRoundedRect(
        rect.center().x() - w * 0.20f,
                        rect.bottom() - h * 0.14f,
                        w * 0.40f,
                        h * 0.10f,
                        w * 0.04f,
                        w * 0.04f
    );

    // Body
    QPolygonF body;
    body << QPointF(rect.center().x() - w * 0.14f, rect.bottom() - h * 0.14f)
    << QPointF(rect.center().x() + w * 0.14f, rect.bottom() - h * 0.14f)
    << QPointF(rect.center().x() + w * 0.18f, rect.top() + h * 0.36f)
    << QPointF(rect.center().x() - w * 0.18f, rect.top() + h * 0.36f);

    path.addPolygon(body);

    // Crown points
    QPolygonF crown;
    crown << QPointF(rect.center().x() - w * 0.20f, rect.top() + h * 0.38f)
    << QPointF(rect.center().x() - w * 0.14f, rect.top() + h * 0.16f)
    << QPointF(rect.center().x() - w * 0.06f, rect.top() + h * 0.32f)
    << QPointF(rect.center().x(), rect.top() + h * 0.08f)
    << QPointF(rect.center().x() + w * 0.06f, rect.top() + h * 0.32f)
    << QPointF(rect.center().x() + w * 0.14f, rect.top() + h * 0.16f)
    << QPointF(rect.center().x() + w * 0.20f, rect.top() + h * 0.38f);

    path.addPolygon(crown);

    // Orb
    path.addEllipse(
        rect.center().x() - w * 0.05f,
                    rect.top() + h * 0.00f,
                    w * 0.10f,
                    w * 0.10f
    );

    p.drawPath(path);
}

void ChessBoardWidget::drawKing(QPainter &p, const QRect &rect, const Piece &piece)
{
    Q_UNUSED(piece);

    QPainterPath path;

    float w = rect.width();
    float h = rect.height();

    // Base
    path.addRoundedRect(
        rect.center().x() - w * 0.20f,
                        rect.bottom() - h * 0.14f,
                        w * 0.40f,
                        h * 0.10f,
                        w * 0.04f,
                        w * 0.04f
    );

    // Body
    QPolygonF body;
    body << QPointF(rect.center().x() - w * 0.15f, rect.bottom() - h * 0.14f)
    << QPointF(rect.center().x() + w * 0.15f, rect.bottom() - h * 0.14f)
    << QPointF(rect.center().x() + w * 0.18f, rect.top() + h * 0.34f)
    << QPointF(rect.center().x() - w * 0.18f, rect.top() + h * 0.34f);

    path.addPolygon(body);

    // Crown/dome
    path.addEllipse(
        rect.center().x() - w * 0.12f,
                    rect.top() + h * 0.20f,
                    w * 0.24f,
                    h * 0.18f
    );

    // Cross vertical
    path.addRect(
        rect.center().x() - w * 0.025f,
                 rect.top() + h * 0.00f,
                 w * 0.05f,
                 h * 0.18f
    );

    // Cross horizontal
    path.addRect(
        rect.center().x() - w * 0.08f,
                 rect.top() + h * 0.06f,
                 w * 0.16f,
                 w * 0.05f
    );

    p.drawPath(path);
}

/**
 * @brief Draws all pieces currently on the board.
 *
 * @param painter
 * The QPainter used for drawing.
 *
 * @details
 * This loops over all 64 squares and draws any non-empty piece.
 *
 * @note
 * This uses the live ChessPosition object owned by the widget.
 *
 * @note
 * This function should only be called on the GUI thread.
 */
void ChessBoardWidget::drawPieces(QPainter &painter)
{
    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            if (!position.board[r][c].isEmpty())
            {
                drawPiece(painter, squareRect(r, c), position.board[r][c]);
            }
        }
    }
}

/**
 * @brief Draws a single chess piece.
 *
 * @param painter
 * The QPainter used for drawing.
 *
 * @param rect
 * The screen rectangle for the square containing the piece.
 *
 * @param piece
 * The piece to draw.
 *
 * @details
 * This selects the correct piece drawing function based on piece type.
 *
 * @note
 * The current piece graphics are simple vector shapes.
 *
 * @note
 * Future improvements could use images or a more detailed piece set.
 */
void ChessBoardWidget::drawPiece(QPainter &painter, const QRect &rect, const Piece &piece)
{
    if (piece.isEmpty())
        return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor fill;
    QColor outline;

    if (piece.color == Color::White)
    {
        fill = QColor(245, 245, 245);
        outline = QColor(30, 30, 30);
    }
    else
    {
        fill = QColor(35, 35, 35);
        outline = QColor(210, 210, 210);
    }

    painter.setPen(QPen(outline, qMax(2.0f, rect.width() * 0.01f)));
    painter.setBrush(fill);

    switch (piece.type)
    {
        case PieceType::Pawn:
            drawPawn(painter, rect, piece);
            break;

        case PieceType::Rook:
            drawRook(painter, rect, piece);
            break;

        case PieceType::Knight:
            drawKnight(painter, rect, piece);
            break;

        case PieceType::Bishop:
            drawBishop(painter, rect, piece);
            break;

        case PieceType::Queen:
            drawQueen(painter, rect, piece);
            break;

        case PieceType::King:
            drawKing(painter, rect, piece);
            break;

        case PieceType::None:
            break;
    }
}

/**
 * @brief Draws selection highlights, legal moves, check highlights, and game-over text.
 *
 * @param painter
 * The QPainter used for drawing.
 *
 * @details
 * During normal play, this draws:
 * - the selected square,
 * - legal destination squares,
 * - the king if the side to move is in check.
 *
 * If the game is over, this draws the final status text on the board.
 *
 * @note
 * This function is UI-only.
 *
 * @note
 * It reads the current game state but does not modify it.
 */
void ChessBoardWidget::drawSelection(QPainter &painter)
{
    if (!gameOver)
    {
        if (selectedRow >= 0 && selectedCol >= 0)
        {
            const QRect selected = squareRect(selectedRow, selectedCol);

            painter.setPen(QPen(QColor(255, 215, 0), 3));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(selected.adjusted(2, 2, -2, -2));

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(120, 255, 120, 180));

            const int cs = cellSize();

            for (const Move &m : legalTargets)
            {
                const int r = m.toRow;
                const int c = m.toCol;

                const QRect rect = squareRect(r, c);
                const QPointF center = rect.center();

                painter.drawEllipse(center, cs * 0.25f, cs * 0.25f);
            }
        }

        // Highlight the king when it is in check.
        if (ChessEngine::isInCheck(position, position.sideToMove))
        {
            int kingRow = -1;
            int kingCol = -1;

            if (ChessEngine::findKing(position, position.sideToMove, kingRow, kingCol))
            {
                const QRect kingRect = squareRect(kingRow, kingCol);

                painter.setPen(QPen(QColor(255, 0, 0), 4));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(kingRect.adjusted(2, 2, -2, -2));
            }
        }
    }
    else
    {
        painter.setPen(QColor(255, 255, 255));

        QFont font = painter.font();
        font.setPointSize(24);
        font.setBold(true);
        painter.setFont(font);

        painter.drawText(
            QRect(0, 0, width(), height()),
                         Qt::AlignCenter,
                         statusText()
        );
    }
}

/**
 * @brief Handles painting of the widget.
 *
 * @param event
 * The paint event.
 *
 * @details
 * This is the main paint entry point.
 *
 * It draws:
 * 1. the board,
 * 2. the pieces,
 * 3. selection/check/game-over overlays.
 *
 * @note
 * This function runs on the GUI thread.
 *
 * @note
 * It should be kept reasonably fast because it may be called often.
 */
void ChessBoardWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    drawBoard(painter);
    drawPieces(painter);
    drawSelection(painter);
}

// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------

/**
 * @brief Handles mouse clicks on the board.
 *
 * @param event
 * The mouse event.
 *
 * @details
 * This function:
 * 1. Converts the mouse position to a board square.
 * 2. If a piece is already selected, tries to move it to the clicked square.
 * 3. If the clicked square contains another own piece, selects that piece.
 * 4. Otherwise deselects.
 *
 * @note
 * This function ignores input while:
 * - the game is over,
 * - the AI is thinking.
 *
 * @note
 * This function only allows moves that are in legalTargets.
 *
 * @note
 * This runs on the GUI thread.
 */
void ChessBoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (gameOver || aiThinking)
        return;

    const int cs = cellSize();
    const int boardSize = cs * 8;
    const int offsetX = (width() - boardSize) / 2;
    const int offsetY = (height() - boardSize) / 2;

    const QPoint local = event->pos();
    const int x = local.x() - offsetX;
    const int y = local.y() - offsetY;

    if (x < 0 || y < 0 || x >= boardSize || y >= boardSize)
    {
        deselect();
        return;
    }

    const int col = x / cs;
    const int row = y / cs;

    if (row < 0 || row >= 8 || col < 0 || col >= 8)
    {
        deselect();
        return;
    }

    // If a piece is already selected, try to move it.
    if (selectedRow >= 0 && selectedCol >= 0)
    {
        for (const Move &m : legalTargets)
        {
            if (m.toRow == row && m.toCol == col)
            {
                makeMove(selectedRow, selectedCol, row, col);
                return;
            }
        }

        // Clicking another own piece can reselect it.
        if (!position.board[row][col].isEmpty() &&
            position.board[row][col].color == position.sideToMove)
        {
            selectPiece(row, col);
            return;
        }

        deselect();
        return;
    }

    // No piece selected yet: select an own piece if clicked.
    if (!position.board[row][col].isEmpty() &&
        position.board[row][col].color == position.sideToMove)
    {
        selectPiece(row, col);
    }
}
/**
 * @brief Handles keyboard input for the board widget.
 *
 * @param event
 * The key event.
 *
 * @details
 * Currently supports:
 * - Escape: deselect the current piece.
 *
 * @note
 * Global shortcuts such as New Game and Undo are handled by MainWindow.
 *
 * @note
 * This function runs on the GUI thread.
 */
void ChessBoardWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        deselect();
        return;
    }

    QWidget::keyPressEvent(event);
}

//-----------------------------------------------------------
// undo last turn
//-----------------------------------------------------------

/**
 * @brief Undoes the last move, or the last full human/AI turn if AI is enabled.
 *
 * @details
 * This function reverses the current move using:
 * - moveHistory,
 * - undoStack,
 * - positionKeyHistory,
 * - positionCounts.
 *
 * It undoes:
 * - the board move,
 * - the side to move,
 * - the move history entry,
 * - the undo stack entry,
 * - the repetition count for the removed position.
 *
 * If AI is enabled, it may undo two moves so that control returns to the
 * human player.
 *
 * Example with AI playing Black:
 *
 * Human White moves.
 * AI Black moves.
 *
 * User presses Undo.
 *
 * The widget first undoes AI Black's move.
 * That would leave Black to move, which is the AI side.
 * So it also undoes the previous White move.
 *
 * Result:
 * White is to move again.
 *
 * @note
 * This function does nothing while aiThinking is true.
 *
 * @note
 * After undoing, updateGameStatus() recalculates checkmate, stalemate,
 * repetition, and normal game state.
 *
 * @note
 * If it is still the AI's turn after undoing, maybeScheduleAiMove() may
 * schedule another AI search.
 */
void ChessBoardWidget::undoLastTurn()
{
    if (aiThinking)
        return;

    if (undoStack.empty())
        return;

    auto undoOneMove = [this]()
    {
        const MoveRecord record = moveHistory.back();
        const ChessEngine::SavedMove saved = undoStack.back();

        moveHistory.pop_back();
        undoStack.pop_back();

        if (!positionKeyHistory.empty())
        {
            const std::string key = positionKeyHistory.back();
            positionKeyHistory.pop_back();

            std::map<std::string, int>::iterator it = positionCounts.find(key);

            if (it != positionCounts.end())
            {
                it->second--;

                if (it->second <= 0)
                    positionCounts.erase(it);
            }
        }

        //undo one move
        ChessEngine::undoMove(
            position,
            saved,
            record.fromRow,
            record.fromCol,
            record.toRow,
            record.toCol
        );

        halfMoveClock = record.halfMoveClockBeforeMove;

        position.sideToMove = ChessEngine::opponent(position.sideToMove);
    };

    undoOneMove();


    // If AI is enabled, undo enough moves so that the human gets control back.
    if (aiEnabled &&
        position.sideToMove == aiColor &&
        !undoStack.empty())
    {
        undoOneMove();
    }


    deselect();
    updateGameStatus();

    maybeScheduleAiMove();
}


//-----------------------------------------------------
// record moves and history
//-----------------------------------------------------

/**
 * @brief Converts a single MoveRecord into short human-readable move text.
 *
 * @param m
 * The move record to format.
 *
 * @return
 * A move string such as:
 * - "e4",
 * - "Nf3",
 * - "exd5",
 * - "O-O",
 * - "O-O-O",
 * - "e8=Q".
 *
 * @details
 * The current implementation produces simple notation.
 * It does not include:
 * - check symbols,
 * - mate symbols,
 * - disambiguation,
 * - full standard algebraic notation.
 *
 * @note
 * This function is shared by:
 * - lastMoveText(),
 * - moveHistoryText(),
 * - moveHistoryTailText().
 */
QString ChessBoardWidget::moveRecordText(const MoveRecord &m) const
{
    QString move;

    if (m.castle)
    {
        if (m.toCol == 6)
            move = QString("O-O");
        else
            move = QString("O-O-O");
    }
    else
    {
        move = squareName(m.fromRow, m.fromCol);

        if (!m.captured.isEmpty())
            move += QString("x");

        move += squareName(m.toRow, m.toCol);
    }

    if (m.enPassant)
        move += QString(" e.p.");

    if (m.promotion == PieceType::Queen)
        move += QString("=Q");

    return move;
}

/**
 * @brief Returns the text of the most recent move.
 *
 * @return
 * The formatted text of the last move, or an empty string if no move exists.
 *
 * @details
 * This is used to display the AI's last move in the status bar, for example:
 *
 * "AI move: Nf6"
 *
 * @note
 * This returns the last move in the current move history.
 * If the user undoes moves, this will reflect the new last move.
 */
QString ChessBoardWidget::lastMoveText() const
{
    if (moveHistory.empty())
        return QString();

    return moveRecordText(moveHistory.back());
}

/**
 * @brief Returns the full move history as a single string.
 *
 * @return
 * A formatted move history string.
 *
 * @details
 * The history is formatted in pairs:
 *
 * 1. e4 e5
 * 2. Nf3 Nc6
 *
 * @note
 * This can become large in long games.
 *
 * @note
 * Use moveHistoryTailText() for UI dialogs that should show only recent moves.
 */
QString ChessBoardWidget::moveHistoryText() const
{
    QString text;

    for (std::size_t i = 0; i < moveHistory.size(); ++i)
    {
        const MoveRecord &m = moveHistory[i];

        const int moveNumber = static_cast<int>(i / 2) + 1;

        if (i % 2 == 0)
            text += QString::number(moveNumber) + QString(". ");

        text += moveRecordText(m);
        text += QString("  ");
    }

    return text;
}

/**
 * @brief Returns the number of half-moves stored in the move history.
 *
 * @return
 * The number of MoveRecord entries currently stored.
 *
 * @details
 * One full move normally consists of two half-moves:
 * - one White move,
 * - one Black move.
 *
 * @note
 * This is useful for UI display logic, for example deciding whether to show
 * a truncated history.
 */
std::size_t ChessBoardWidget::moveHistorySize() const
{
    return moveHistory.size();
}

/**
 * @brief Returns only the last N full moves from the move history.
 *
 * @param maxFullMoves
 * Maximum number of full moves to include.
 * If zero or negative, the full history is returned.
 *
 * @return
 * A formatted string containing the most recent moves.
 *
 * @details
 * This is intended for the move history dialog.
 * It avoids displaying hundreds of moves at once.
 *
 * @note
 * The starting index is aligned to a full-move boundary where possible,
 * so the displayed history usually starts with a White move.
 *
 * @note
 * This function is UI-oriented and does not affect the chess engine.
 */
QString ChessBoardWidget::moveHistoryTailText(int maxFullMoves) const
{
    if (moveHistory.empty())
        return QString();

    if (maxFullMoves <= 0)
        return moveHistoryText();

    const std::size_t wantedHalfMoves =
    static_cast<std::size_t>(maxFullMoves) * 2;

    if (moveHistory.size() <= wantedHalfMoves)
        return moveHistoryText();

    std::size_t start = moveHistory.size() - wantedHalfMoves;

    // Align to a full-move boundary.
    // Index 0 = White's first move, index 1 = Black's first move, etc.
    if (start % 2 != 0)
        ++start;

    if (start >= moveHistory.size())
        return QString();

    QString text;

    for (std::size_t i = start; i < moveHistory.size(); ++i)
    {
        const MoveRecord &m = moveHistory[i];

        const int moveNumber = static_cast<int>(i / 2) + 1;

        if (i % 2 == 0)
            text += QString::number(moveNumber) + QString(". ");

        text += moveRecordText(m);
        text += QString("  ");
    }

    return text;
}

/**
 * @brief Prints the full move history to the debug console.
 *
 * @details
 * This is mainly useful during development and testing.
 *
 * @note
 * This is not the recommended user-facing move history display.
 * Use the MainWindow move history dialog for that.
 */
void ChessBoardWidget::printMoveHistory() const
{
    qDebug().noquote() << moveHistoryText();
}

/**
 * @brief Converts a board square to chess algebraic notation.
 *
 * @param row
 * Board row, 0 to 7.
 *
 * @param col
 * Board column, 0 to 7.
 *
 * @return
 * A square name such as "e4".
 *
 * @details
 * In this project:
 * - row 0 is Black's back rank,
 * - row 7 is White's back rank.
 *
 * The rank is therefore calculated as 8 - row.
 *
 * @note
 * This function is used to format move history text.
 */
QString ChessBoardWidget::squareName(int row, int col) const
{
    static const char files[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

    const int rank = 8 - row;

    return QString(QChar(files[col])) + QString::number(rank);
}

/**
 * @brief Sets the callback used to update the MainWindow status label.
 *
 * @param callback
 * A callable object that receives a human-readable status string.
 *
 * @details
 * This callback is invoked whenever the game status changes.
 *
 * @note
 * The callback should update GUI elements only.
 * It should not perform expensive work or start blocking operations.
 *
 * @note
 * This keeps the board widget loosely coupled from the MainWindow.
 */
void ChessBoardWidget::setStatusCallback(std::function<void(const QString &)> callback)
{
    statusCallback = std::move(callback);
}

/**
 * @brief Returns a human-readable description of the current game state.
 *
 * @return
 * A status string such as:
 * - "White to move",
 * - "Black to move - Black is in check",
 * - "White wins by checkmate.",
 * - "Stalemate - draw.",
 * - "Draw by threefold repetition."
 *
 * @details
 * If the last move was made by the AI, the status may also include:
 *
 * "AI move: e4"
 *
 * @note
 * This function is used by both:
 * - the status callback,
 * - the in-board game-over text.
 *
 * @note
 * This function should remain lightweight because it may be called during
 * painting or status updates.
 */
QString ChessBoardWidget::statusText() const
{
    QString side;

    if (position.sideToMove == Color::White)
        side = QString("White");
    else
        side = QString("Black");

    if (gameOver)
    {
        if (gameResult == GameResult::Checkmate)
        {
            QString winnerSide;

            if (winner == Color::White)
                winnerSide = QString("White");
            else
                winnerSide = QString("Black");

            return winnerSide + QString(" wins by checkmate.");
        }

        if (gameResult == GameResult::Stalemate)
            return QString("Stalemate - draw.");

        if (gameResult == GameResult::Repetition)
            return QString("Draw by threefold repetition.");

        if (gameResult == GameResult::FiftyMove)
            return QString("Draw by 50-move rule.");

        if (gameResult == GameResult::InsufficientMaterial)
            return QString("Draw by insufficient material.");

        return QString("Game over.");
    }

    QString text = side + QString(" to move");

    if (ChessEngine::isInCheck(position, position.sideToMove))
        text += QString(" - ") + side + QString(" is in check");

    if (!moveHistory.empty() && moveHistory.back().aiMove)
    {
        text += QString("  AI move: ") + lastMoveText();
    }

    if (!lastHint.isEmpty() && !gameOver)
    {
        text += QString("  Hint: ") + lastHint;
    }


    return text;
}


/**
 * @brief Reports whether the game has ended.
 *
 * @return
 * true if the game is over, otherwise false.
 *
 * @details
 * The game is over when:
 * - checkmate occurs,
 * - stalemate occurs,
 * - threefold repetition has been detected,
 * - or another terminal state is later added.
 *
 * @note
 * This is used to block further user interaction.
 */
bool ChessBoardWidget::isGameOver() const
{
    return gameOver;
}

/**
 * @brief Recalculates the current game result from the position and history.
 *
 * @details
 * This function checks:
 * 1. Whether the side to move has any legal moves.
 * 2. Whether the side to move is in check.
 * 3. Whether the current position has occurred three times.
 *
 * Result priority:
 * - checkmate,
 * - stalemate,
 * - threefold repetition,
 * - check,
 * - ongoing.
 *
 * It then:
 * - updates gameOver,
 * - updates gameResult,
 * - updates winner,
 * - repaints the widget,
 * - invokes the status callback.
 *
 * @note
 * This must be called after every move or undo.
 *
 * @note
 * This function should run on the GUI thread.
 */
void ChessBoardWidget::updateGameStatus()
{
    const bool anyLegalMove =
    ChessEngine::hasAnyLegalMove(position, position.sideToMove);

    const bool inCheck =
    ChessEngine::isInCheck(position, position.sideToMove);

    const bool repetitionDraw =
    repetitionDrawEnabled &&
    isCurrentPositionRepeatedThreeTimes();

    const bool fiftyMoveDraw =
    fiftyMoveDrawEnabled &&
    halfMoveClock >= 100;

    //testing
    //const bool insufficientMaterialDraw =false;

    const bool insufficientMaterialDraw = insufficientMaterialDrawEnabled &&
    ChessEngine::isInsufficientMaterial(position);

    if (!anyLegalMove)
    {
        gameOver = true;

        if (inCheck)
        {
            gameResult = GameResult::Checkmate;
            winner = ChessEngine::opponent(position.sideToMove);
        }
        else
        {
            gameResult = GameResult::Stalemate;
            winner = position.sideToMove;
        }
    }
    else if (repetitionDraw)
    {
        gameOver = true;
        gameResult = GameResult::Repetition;
        winner = position.sideToMove;
    }
    else if (fiftyMoveDraw)
    {
        gameOver = true;
        gameResult = GameResult::FiftyMove;
        winner = position.sideToMove;
    }
    else if (insufficientMaterialDraw)
    {
        gameOver = true;
        gameResult = GameResult::InsufficientMaterial;
        winner = position.sideToMove;
    }
    else
    {
        gameOver = false;

        if (inCheck)
            gameResult = GameResult::Check;
        else
            gameResult = GameResult::Ongoing;

        winner = position.sideToMove;
    }

    update();

    if (statusCallback)
        statusCallback(statusText());
}

/**
 * @brief Checks whether the current position has occurred at least three times.
 *
 * @return
 * true if the current position key has a count of three or more, otherwise false.
 *
 * @details
 * This is used to implement the threefold repetition draw rule.
 *
 * The position key includes:
 * - board placement,
 * - side to move,
 * - castling rights,
 * - en-passant target.
 *
 * @note
 * This function is UI/game-state oriented.
 *
 * @note
 * The repetition count is maintained by ChessBoardWidget, not ChessEngine,
 * because repetition is a game-history rule rather than a pure position rule.
 */
bool ChessBoardWidget::isCurrentPositionRepeatedThreeTimes() const
{
    const std::string key = position.key();

    auto it = positionCounts.find(key);

    if (it != positionCounts.end())
        return it->second >= 3;

    return false;
}

//Game draw options threefold repetition, 50 moves, material (user settings)
void ChessBoardWidget::setRepetitionDrawEnabled(bool enabled)
{
    repetitionDrawEnabled = enabled;
    updateGameStatus();
}

void ChessBoardWidget::setFiftyMoveDrawEnabled(bool enabled)
{
    fiftyMoveDrawEnabled = enabled;
    updateGameStatus();
}

void ChessBoardWidget::setInsufficientMaterialDrawEnabled(bool enabled)
{
    insufficientMaterialDrawEnabled = enabled;
    updateGameStatus();
}

bool ChessBoardWidget::repetitionDrawEnabledValue() const
{
    return repetitionDrawEnabled;
}

bool ChessBoardWidget::fiftyMoveDrawEnabledValue() const
{
    return fiftyMoveDrawEnabled;
}

bool ChessBoardWidget::insufficientMaterialDrawEnabledValue() const
{
    return insufficientMaterialDrawEnabled;
}


/**
 * @brief Schedules a request hint (white best move).
 *
 * @note similar to maybeScheduleAiMove()
 * The important difference is that the GUI side will not call executeMove()
 * The Key difference from AI move:
 *
 * AI move:
 *    search best move
 *    execute move on board
 *
 * Hint:
 *    search best move
 *    show move text
 *    do not execute move
 *
 */
void ChessBoardWidget::requestHint()
{
    if (gameOver)
        return;

    if (hintThinking)
        return;

    if (aiThinking)
        return;

    ChessPosition snapshot = position;

    const Color color = position.sideToMove;
    const int depth = aiDepth;

    const int requestId = ++hintRequestId;
    hintActiveRequestId = requestId;

    std::promise<Move> promise;

    hintFuture = std::future<Move>();
    hintFuture = promise.get_future();

    if (hintThread.joinable())
        hintThread.join();

    hintThinking = true;

    hintThread = std::thread(
        [snapshot = std::move(snapshot), depth, color, promise = std::move(promise)]() mutable
        {
            try
            {
                Move best = ChessEngine::findBestMove(snapshot, color, depth);
                promise.set_value(best);
            }
            catch (...)
            {
                promise.set_exception(std::current_exception());
            }
        });

    pollHintResult();
}

/**
 * @brief Checks whether the  worker thread has finished and stores hint without changing the board.
 *
 * @details
 * This function is always called on the GUI thread.
 *
 * It uses std::future::wait_for() with a timeout of zero milliseconds:
 *
 * hintFuture.wait_for(std::chrono::milliseconds(0))
 */
void ChessBoardWidget::pollHintResult()
{
    if (!hintFuture.valid())
    {
        hintThinking = false;
        return;
    }

    if (hintFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        QTimer::singleShot(
            20,
            this,
            [this]()
            {
                pollHintResult();
            });

        return;
    }

    if (hintThread.joinable())
        hintThread.join();

    Move best;
    bool ok = true;

    try
    {
        best = hintFuture.get();
    }
    catch (...)
    {
        ok = false;
    }

    hintFuture = std::future<Move>();

    if (hintActiveRequestId != hintRequestId)
    {
        hintThinking = false;
        return;
    }

    if (ok)
    {
        lastHint = squareName(best.fromRow, best.fromCol) +
        squareName(best.toRow, best.toCol);

        update();

        if (statusCallback)
            statusCallback(statusText());
    }

    hintThinking = false;
}

QString ChessBoardWidget::hintText() const
{
    return lastHint;
}





