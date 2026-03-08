cmake_minimum_required(VERSION 3.22)
project(GLAD C)

add_library(GLAD STATIC src/glad.c)
target_include_directories(GLAD PUBLIC include)
target_compile_options(GLAD PRIVATE -Wno-pedantic)
