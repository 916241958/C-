cmake_minimum_required(VERSION 3.15)
project(threadPool)

set(CMAKE_CXX_STANDARD 17)

add_executable(threadPool main.cpp threadpool.cpp threadpool.h)