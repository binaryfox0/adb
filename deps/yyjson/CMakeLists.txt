project(yyjson)
add_library(yyjson src/yyjson.c)
target_include_directories(yyjson PUBLIC include)
