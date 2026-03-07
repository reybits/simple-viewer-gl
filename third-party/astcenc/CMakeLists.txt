cmake_minimum_required(VERSION 3.22)
project(AstcDec)

file(GLOB ASTCENC_SRCS "src/astcenc_*.cpp")

add_library(${PROJECT_NAME} STATIC ${ASTCENC_SRCS})

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Decompress-only build: exclude all compression code
target_compile_definitions(${PROJECT_NAME} PRIVATE ASTCENC_DECOMPRESS_ONLY)

# SIMD flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ASTCENC_NEON=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(${PROJECT_NAME} PRIVATE -msse4.1)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ASTCENC_SSE=41)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE ASTCENC_NEON=0 ASTCENC_SSE=0)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-parameter)
