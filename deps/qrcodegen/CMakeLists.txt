cmake_minimum_required(VERSION 3.15)
project(qrcodegen)
add_library(qrcodegen src/qrcodegen.c)
target_include_directories(qrcodegen PUBLIC include/)
