cmake_minimum_required(VERSION 3.16)
project(mdns)
add_library(mdns src/mdns.c)
target_include_directories(mdns PRIVATE include/)
