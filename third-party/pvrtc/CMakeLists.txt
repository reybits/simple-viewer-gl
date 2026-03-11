cmake_minimum_required(VERSION 3.22)
project(PvrDec)

add_library(${PROJECT_NAME} STATIC src/PVRTDecompress.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC include)
