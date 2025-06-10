cmake_minimum_required(VERSION 3.25)

project("los")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(INC_DIR "include")
set(SRC_DIR "src")

find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${INC_DIR})

file(GLOB_RECURSE INC_FILES "${INC_DIR}/*.h")
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.c")

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(
${PROJECT_NAME}
PRIVATE
    ${Stb_INCLUDE_DIR}
)

target_link_libraries(
${PROJECT_NAME}
PRIVATE
    GLEW::GLEW
    glfw
)

target_precompile_headers(
${PROJECT_NAME}
PRIVATE
    "${INC_DIR}/pch.h"
)

add_custom_command(
TARGET ${PROJECT_NAME} POST_BUILD
COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/$<CONFIG>/assets"
)
