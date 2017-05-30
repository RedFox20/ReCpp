cmake_minimum_required(VERSION 2.8)
project(ReCpp)

set(CMAKE_C_COMPILER   "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_CXX_FLAGS    "-std=c++1z -Wall -march=native ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

message("DEBUG   FLAGS: ${CMAKE_CXX_FLAGS_DEBUG}")
message("RELEASE FLAGS: ${CMAKE_CXX_FLAGS_RELEASE}")
message("LINKER  FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")

set(RPP_SOURCES
    rpp/strview.cpp
    rpp/strview.h
    rpp/file_io.cpp
    rpp/file_io.h
    rpp/vec.cpp
    rpp/vec.h
    rpp/debugging.cpp
    rpp/debugging.h
    rpp/thread_pool.cpp
    rpp/thread_pool.h
    rpp/timer.cpp
    rpp/timer.h)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
add_library(${PROJECT_NAME} STATIC ${RPP_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})




