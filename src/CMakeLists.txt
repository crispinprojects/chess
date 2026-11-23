cmake_minimum_required(VERSION 3.20)
project(chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(chess
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/chessboardwidget.cpp
    src/chessboardwidget.h
    src/chessengine.h
    src/chessengine.cpp
    src/chessposition.h
    src/chessposition.cpp
)


target_link_libraries(chess PRIVATE Qt6::Widgets)
