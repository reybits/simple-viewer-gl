include_directories(include)

set(FMTLIB_SRCS
    "src/format.cc"
    "src/os.cc")

add_library(FmtLib ${FMTLIB_SRCS})
target_compile_options(FmtLib PRIVATE -Wno-error -Wno-deprecated-declarations)
target_include_directories(FmtLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
