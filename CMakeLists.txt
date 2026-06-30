cmake_minimum_required(VERSION 3.10)
project(MiniTetris LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tetris WIN32 tetris.cpp)
# For console build instead of GUI app, remove WIN32

# Installation target
install(TARGETS tetris RUNTIME DESTINATION bin)