cmake_minimum_required(VERSION 3.0)
project(ReCpp)

set(RPP_CXX_FLAGS -std=c++14 -Wall -march=native)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(RPP_CXX_FLAGS ${RPP_CXX_FLAGS} -DDEBUG)
endif()

message("ReCpp ${CMAKE_BUILD_TYPE}: ${RPP_CXX_FLAGS} ")

####################################################

add_library(ReCpp
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
        rpp/timer.h
        rpp/sockets.cpp
        rpp/sockets.h
        rpp/tests.cpp
        rpp/tests.h
        rpp/delegate.h
        rpp/obfuscated_string.h
        rpp/binary_stream.cpp
        rpp/binary_stream.h
        rpp/binary_serializer.cpp
        rpp/binary_serializer.h)

target_compile_definitions(ReCpp PUBLIC RPP_TESTS_DEFINE_MAIN=0)
target_include_directories(ReCpp PUBLIC ".")
target_compile_options(ReCpp PRIVATE ${RPP_CXX_FLAGS})
target_link_libraries(ReCpp PUBLIC pthread)
set(TARGET ReCpp RUNTIME_OUTPUT_DIRECTORY ./bin)

####################################################

file(GLOB_RECURSE RPP_TESTS tests/*.cpp)
add_executable(RppTests ${RPP_TESTS})

target_include_directories(RppTests PUBLIC ".")
target_compile_options(RppTests PRIVATE ${RPP_CXX_FLAGS})
target_link_libraries(RppTests ReCpp)
set(TARGET RppTests RUNTIME_OUTPUT_DIRECTORY ./bin)

####################################################

set(LINK_DIRECTORIES ${LINK_DIRECTORIES} ./bin)
