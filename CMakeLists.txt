cmake_minimum_required(VERSION 4.2)

project (MiniDbg)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(mydbg
    src/main.cpp
)

target_include_directories(mydbg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(hello
    examples/hello.c
)
target_compile_options(hello PRIVATE -g -O0)


add_library(linenoise STATIC ext/linenoise/linenoise.c)
target_include_directories(linenoise PUBLIC ext/linenoise)
target_compile_options(linenoise PRIVATE -w)
target_link_libraries(mydbg PRIVATE linenoise)