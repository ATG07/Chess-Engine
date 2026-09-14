cmake_minimum_required(VERSION 3.16)

project(ChessEngine LANGUAGES CXX)

add_library(chess_engine
    src/board.cpp
    src/engine.cpp
    src/eval.cpp
    src/uci.cpp
)

target_include_directories(chess_engine PUBLIC include)
target_compile_features(chess_engine PUBLIC cxx_std_20)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
