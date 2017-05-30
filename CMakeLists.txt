cmake_minimum_required(VERSION 3.0)
project(ReCpp)

add_library(${PROJECT_NAME} STATIC
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

target_include_directories(ReCpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set(TARGET ReCpp CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(LINK_DIRECTORIES ${LINK_DIRECTORIES} ${CMAKE_CURRENT_SOURCE_DIR}/bin)

target_compile_options(ReCpp PRIVATE -std=c++1z -Wall -march=native -fPIC)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ReCpp PRIVATE -DDEBUG)
endif()

get_target_property(options ReCpp COMPILE_OPTIONS)
message("ReCpp ${CMAKE_BUILD_TYPE}: ${options} ")

target_link_libraries(ReCpp PUBLIC pthread)