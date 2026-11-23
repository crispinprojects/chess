/*
 * Copyright 2026 Alan Crispin <crispinalan@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "mainwindow.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFont>
#include <QActionGroup> //CopperSpice supports QActionGroup
#include <QDialogButtonBox>


/**
 * @brief Constructs the main window.
 *
 * Creates the central widget, board, status label, menu bar,
 * and connects the main actions.
 *
 * @param parent Optional parent widget.
 */
MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent)
, message(new QLabel(this))
, board(new ChessBoardWidget(this))
{
    //qDebug() << "mainwindow constructor called";
     uiFontSize = 14;

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    message->setText("Information");

    board->setMinimumSize(640, 640);
    board->setFocus();

    applyUiFont();

    board->setFocusPolicy(Qt::StrongFocus);
    board->setFocus();

    board->setStatusCallback([this](const QString &text)
    {
        message->setText(text);
    });

    updateStatus();
    layout->addWidget(board);
    layout->addWidget(message);
    setCentralWidget(central);

    // -------------------------------------------------------------------------
    // Menu bar
    // -------------------------------------------------------------------------

    auto *menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // --- File menu ---
    auto *fileMenu = menuBar->addMenu("&File");

    auto *quitAction = fileMenu->addAction("&Quit");
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // --- Game menu ---
    auto *gameMenu = menuBar->addMenu("&Game");

    auto *newGameAction = gameMenu->addAction("&New Game");
    newGameAction->setShortcut(QKeySequence(Qt::Key_R));
    connect(newGameAction, &QAction::triggered, this, &MainWindow::reset);

    //AI opponent

    auto *aiAction = gameMenu->addAction("&AI Plays Black");
    aiAction->setCheckable(true);

    connect(aiAction, &QAction::toggled, this, [this](bool on)
    {
        board->setAiEnabled(on);
        board->setAiDepth(aiDepth);
        board->startAiMove();
    });

    // --- AI Depth menu ---
    auto *depthMenu = gameMenu->addMenu("AI Depth");

    auto *depthGroup = new QActionGroup(this);

    for (int d = 1; d <= 6; ++d)
    {
        auto *depthAction = depthMenu->addAction(QString("Depth ") + QString::number(d));
        depthAction->setCheckable(true);
        depthAction->setChecked(d == aiDepth);
        depthGroup->addAction(depthAction);

        connect(depthAction, &QAction::triggered, this, [this, d]()
        {
            aiDepth = d;
            board->setAiDepth(aiDepth);
            board->startAiMove();
        });
    }

    auto *undoAction = gameMenu->addAction("Undo Last Turn");
    undoAction->setShortcut(QKeySequence(Qt::Key_Z));

    connect(undoAction, &QAction::triggered, this, [this]()
    {
        board->undoLastTurn();

    });

    auto *historyAction = gameMenu->addAction("Move History");

    connect(historyAction, &QAction::triggered, this, [this]()
    {
        showMoveHistory();
    });

    // --- Draw rules menu ---
    auto *drawMenu = gameMenu->addMenu("Draw Rules");
    auto *repetitionAction = drawMenu->addAction("Threefold Repetition");
    repetitionAction->setCheckable(true);
    repetitionAction->setChecked(board->repetitionDrawEnabledValue());

    connect(repetitionAction, &QAction::toggled, this, [this](bool on)
    {
        board->setRepetitionDrawEnabled(on);
    });

    auto *fiftyMoveAction = drawMenu->addAction("50-Move Rule");
    fiftyMoveAction->setCheckable(true);
    fiftyMoveAction->setChecked(board->fiftyMoveDrawEnabledValue());

    connect(fiftyMoveAction, &QAction::toggled, this, [this](bool on)
    {
        board->setFiftyMoveDrawEnabled(on);
    });

    auto *materialAction = drawMenu->addAction("Insufficient Material");
    materialAction->setCheckable(true);
    materialAction->setChecked(board->insufficientMaterialDrawEnabledValue());

    connect(materialAction, &QAction::toggled, this, [this](bool on)
    {
        board->setInsufficientMaterialDrawEnabled(on);
    });

    auto *hintAction = gameMenu->addAction("Hint");

    connect(hintAction, &QAction::triggered, this, [this]()
    {
        board->requestHint();
    });

   // --- Help menu ---
    auto *helpMenu = menuBar->addMenu("&Help");

    auto *infoAction = helpMenu->addAction("&Information");

    helpMenu->addSeparator();

    auto *fontAction = helpMenu->addAction("Font Size...");
    fontAction->setFont(uiFont()); // or rely apply
    connect(fontAction, &QAction::triggered, this, [this]() { showFontSettings(); });

    helpMenu->addSeparator();

    auto *aboutAction = helpMenu->addAction("&About");

    connect(infoAction, &QAction::triggered, this, &MainWindow::information);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::about);

    applyUiFont();

    setWindowTitle("Chess");
    resize(680, 680);
}

/**
 * @brief Destroys the main window.
 */
MainWindow::~MainWindow()
{
}

/**
 * @brief Starts a new game.
 */
void MainWindow::reset()
{
    qDebug() << "reset board";
    board->newGame();
}


QFont MainWindow::uiFont() const
{
    QFont f("Sans Serif");
    f.setPointSize(uiFontSize);
    return f;
}

void MainWindow::applyUiFont()
{
    QFont f = uiFont();

    this->setFont(f);

    const QList<QWidget *> widgets = findChildren<QWidget *>();
    for (int i = 0; i < widgets.size(); ++i)
    {
        if (widgets.at(i))
            widgets.at(i)->setFont(f);
    }

    const QList<QAction *> actions = findChildren<QAction *>();
    for (int i = 0; i < actions.size(); ++i)
    {
        if (actions.at(i))
            actions.at(i)->setFont(f);
    }

    if (board)
        board->setFont(f);

    if (message)
        message->setFont(f);

    update();
}

void MainWindow::setUiFontSize(int size)
{
    if (size < 8) size = 8;
    if (size > 48) size = 48;
    if (size == uiFontSize) return;
    uiFontSize = size;
    applyUiFont();
}

/**
 * @brief Shows the font setting dialog.
 */
void MainWindow::showFontSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Font Size");
    dialog.setFont(uiFont());

    auto *layout = new QVBoxLayout(&dialog);

    auto *label = new QLabel("Choose a font point size", &dialog);
    label->setFont(uiFont());

    auto *spin = new QSpinBox(&dialog);
    spin->setFont(uiFont());
    spin->setRange(8, 48);
    spin->setValue(uiFontSize);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
    );

    buttons->setFont(uiFont());

    layout->addWidget(label);
    layout->addWidget(spin);
    layout->addWidget(buttons);

    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        this,
        [this, spin]()
        {
            setUiFontSize(spin->value());
        }
    );

    QObject::connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
    );

    dialog.exec();
}




/**
 * @brief Updates the status label.
 *
 * This uses BoardWidget::getStatusText() and appends the current AI depth.
 */
void MainWindow::updateStatus()
{
    if (board)
    {
        message->setText(board->statusText());
    }
}


/**
 * @brief Shows system information.
 */
void MainWindow::information()
{
    QMessageBox box(this);
    box.setWindowTitle("System Info");
    box.setTextFormat(Qt::RichText);
    box.setFont(uiFont());

    QString msg = "<b> Chess Version: </b>";
    msg.append(PROJECT_VERSION);
    msg.append("<br>");

    msg.append("<b>Qt Version: </b> ");
    msg.append(QT_VERSION_STR);
    msg.append("<br>");
    msg.append("<br>");

    msg.append("<b>Shortcuts</b><br>");
    msg.append("R Key = Reset Game<br>");
    msg.append("Z Key = Undo Last Turn<br>");

    box.setText(msg);
    box.exec();
}

/**
 * @brief Shows the about dialog.
 */
void MainWindow::about()
{
    QMessageBox box(this);
    box.setWindowTitle("About");
    box.setTextFormat(Qt::RichText);
    box.setFont(uiFont());

    QString msg = "A two-player chess game.";
    msg.append("<br>");
    msg.append("You can play against another person or enable the minimax AI to play Black.");
    msg.append("<br>");
    msg.append("You can request a move hint (Game->Hint) but do not rely on this.");
    box.setText(msg);
    box.exec();
}

/**
 * @brief Shows the move history dialog.
 */
void MainWindow::showMoveHistory()
{
    QMessageBox box(this);
    box.setWindowTitle("Move History");
    box.setTextFormat(Qt::RichText);
    box.setFont(uiFont());

    const int maxFullMoves = 30;

    const std::size_t totalHalfMoves = board->moveHistorySize();

    QString msg = "<b>Move History</b>";
    msg.append("<br>");

    if (totalHalfMoves == 0)
    {
        msg.append("<i>No moves yet.</i>");
    }
    else
    {
        const int totalFullMoves = static_cast<int>(totalHalfMoves / 2);

        if (totalHalfMoves > static_cast<std::size_t>(maxFullMoves) * 2)
        {
            msg.append("Showing last ");
            msg.append(QString::number(maxFullMoves));
            msg.append(" full moves of ");
            msg.append(QString::number(totalFullMoves));
            msg.append(" full moves.<br>");
        }

        msg.append(board->moveHistoryTailText(maxFullMoves));
    }

    box.setText(msg);
    box.exec();
}

