cmake_minimum_required(VERSION 3.16)
project(rcpspt_exact)

set(CMAKE_CXX_STANDARD 17)

add_executable(rcpspt_exact src/Main.cc src/Problem.cc src/Parser.cc)
