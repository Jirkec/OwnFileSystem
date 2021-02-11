cmake_minimum_required(VERSION 3.17)
project(semestral C)

set(CMAKE_C_STANDARD 11)

add_executable(semestral main.c ext.c commands.c commands.h)