/*
 * Copyright 2026 Alan Crispin <crispinalan@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QLabel>
#include <QMessageBox>
#include <QDialog>
#include <QSpinBox>
#include <QDebug>
#include <QFont>

#include "chessboardwidget.h"

#define PROJECT_VERSION "0.1.1"

/**
 * @class MainWindow
 * @brief Main application window.
 *
 * This window contains the board widget, status label, menu bar,
 * and AI depth dialog.
 */

class MainWindow : public QMainWindow
{


public:
    /**
     * @brief Constructs the main window.
     *
     * @param parent Optional parent widget.
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief Destroys the main window.
     */
    virtual ~MainWindow();

private:
    QLabel *message;
    ChessBoardWidget *board;

    int aiDepth = 4; //search depth


    /** @brief Starts a new game. */
    void reset();

    /** @brief Updates the status label from the board state. */
    void updateStatus();


    /** @brief Shows system information. */
    void information();

     /** @brief Shows move history. */
    void showMoveHistory();

    /** @brief Shows the about dialog. */
    void about();

    int uiFontSize = 14;
    QFont uiFont() const;
    void applyUiFont();
    void setUiFontSize(int size);
    void showFontSettings();

};

#endif // MAINWINDOW_H
