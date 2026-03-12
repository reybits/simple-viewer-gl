cmake_minimum_required(VERSION 3.15)

set(LUNASVG_VERSION_MAJOR 3)
set(LUNASVG_VERSION_MINOR 5)
set(LUNASVG_VERSION_MICRO 0)

project(lunasvg LANGUAGES CXX VERSION ${LUNASVG_VERSION_MAJOR}.${LUNASVG_VERSION_MINOR}.${LUNASVG_VERSION_MICRO})

set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "Disable plutovg examples" FORCE)
add_subdirectory(plutovg)

set(lunasvg_sources
    source/lunasvg.cpp
    source/graphics.cpp
    source/svgelement.cpp
    source/svggeometryelement.cpp
    source/svglayoutstate.cpp
    source/svgpaintelement.cpp
    source/svgparser.cpp
    source/svgproperty.cpp
    source/svgrenderstate.cpp
    source/svgtextelement.cpp
)

set(lunasvg_headers
    include/lunasvg.h
    source/graphics.h
    source/svgelement.h
    source/svggeometryelement.h
    source/svglayoutstate.h
    source/svgpaintelement.h
    source/svgparserutils.h
    source/svgproperty.h
    source/svgrenderstate.h
    source/svgtextelement.h
)

add_library(lunasvg STATIC ${lunasvg_sources} ${lunasvg_headers})
add_library(lunasvg::lunasvg ALIAS lunasvg)

target_link_libraries(lunasvg PRIVATE plutovg::plutovg)
set_target_properties(lunasvg PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    CXX_STANDARD_REQUIRED ON
    CXX_STANDARD 17
)

target_include_directories(lunasvg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_include_directories(lunasvg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(lunasvg PRIVATE LUNASVG_BUILD)
target_compile_definitions(lunasvg PUBLIC LUNASVG_BUILD_STATIC)
target_compile_options(lunasvg PRIVATE -w)
