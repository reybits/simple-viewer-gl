cmake_minimum_required(VERSION 3.15)

set(PLUTOVG_VERSION_MAJOR 1)
set(PLUTOVG_VERSION_MINOR 3)
set(PLUTOVG_VERSION_MICRO 2)

project(plutovg LANGUAGES C VERSION ${PLUTOVG_VERSION_MAJOR}.${PLUTOVG_VERSION_MINOR}.${PLUTOVG_VERSION_MICRO})

set(plutovg_sources
    source/plutovg-blend.c
    source/plutovg-canvas.c
    source/plutovg-font.c
    source/plutovg-matrix.c
    source/plutovg-paint.c
    source/plutovg-path.c
    source/plutovg-rasterize.c
    source/plutovg-surface.c
    source/plutovg-ft-math.c
    source/plutovg-ft-raster.c
    source/plutovg-ft-stroker.c
)

set(plutovg_headers
    include/plutovg.h
    source/plutovg-private.h
    source/plutovg-utils.h
    source/plutovg-ft-math.h
    source/plutovg-ft-raster.h
    source/plutovg-ft-stroker.h
    source/plutovg-ft-types.h
    source/plutovg-stb-image-write.h
    source/plutovg-stb-image.h
    source/plutovg-stb-truetype.h
)

add_library(plutovg STATIC ${plutovg_sources} ${plutovg_headers})
add_library(plutovg::plutovg ALIAS plutovg)

include(CheckIncludeFile)

set_target_properties(plutovg PROPERTIES
    C_VISIBILITY_PRESET hidden
    C_STANDARD_REQUIRED ON
    C_STANDARD 11
)

target_include_directories(plutovg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_include_directories(plutovg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(plutovg PRIVATE m)
endif()

check_include_file("threads.h" HAVE_THREADS_H)
if(HAVE_THREADS_H)
    target_compile_definitions(plutovg PRIVATE HAVE_THREADS_H)
endif()
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(plutovg PRIVATE Threads::Threads)
endif()
find_library(STDTHREADS_LIBRARY stdthreads)
if(STDTHREADS_LIBRARY)
    target_link_libraries(plutovg PRIVATE stdthreads)
endif()

target_compile_definitions(plutovg PRIVATE PLUTOVG_BUILD)
target_compile_definitions(plutovg PUBLIC PLUTOVG_BUILD_STATIC)
target_compile_options(plutovg PRIVATE -w)
