cmake_minimum_required(VERSION 3.20)
project(radar-hack CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(radar-hack
        src/main.cpp
        src/process.cpp
        src/entity.cpp
)

target_compile_definitions(radar-hack PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    target_compile_options(radar-hack PRIVATE /W4)
else()
    target_compile_options(radar-hack PRIVATE -Wall -Wextra)
endif()

# Console subsystem (no WIN32 flag): just a console window.
set_target_properties(radar-hack PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
