include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB INCLUDES "imgui/*.h" "filebrowser/*.h")
file(GLOB_RECURSE IMGUI_SRCS "imgui/*.cpp" "filebrowser/*.cpp")

add_library(ImGui STATIC ${IMGUI_SRCS})
target_include_directories(ImGui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
