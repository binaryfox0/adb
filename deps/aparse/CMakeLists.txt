cmake_minimum_required(VERSION 3.15)
project(aparse)
add_library(aparse src/aparse.c)
target_include_directories(aparse PUBLIC include/)
