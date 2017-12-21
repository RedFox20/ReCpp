cmake_minimum_required(VERSION 3.0)
project(ReCpp)

if(ANDROID OR ANDROID_NDK)
elseif(WIN32 OR MSVC)
elseif(APPLE)
elseif(UNIX)
    set(LINUX TRUE)
else()
    message(FATAL_ERROR "Unsupported Platform: ReCpp/CMakeLists.txt needs updating!")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CLANG TRUE)
elseif(MSVC)
else()
    message(FATAL_ERROR "Unsupported CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(CLANG)
    add_definitions(-Wall)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    if(LINUX)
        add_definitions(-march=native)
    endif()
elseif(MSVC)
    # /MD - MultiThreaded Dynamic CRT
    # /wd4996 - Disable deprecation warning
    # /wd4819 - Disable code page warning (OpenCV)
    # /wd4251 - Disable dll-interface warning
    # /wd4275 - Disable dll-interface base class warning
    # /W3 - warning level 3
    # /Zi - Debug Database
    # /Oi - Intrinsics Enabled
    add_definitions(/std:c++latest /arch:AVX2 /Oi)
    add_definitions(/MD /wd4996 /wd4819 /wd4251 /wd4275 /W3 /Zi)
    add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
    
    foreach(MODE "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
        string(REPLACE "/MDd" "/MD" TMP "${CMAKE_C_FLAGS${MODE}}")
        set(CMAKE_C_FLAGS${MODE} "${TMP}" CACHE STRING "" FORCE)
        message(STATUS "C_${MODE}=${CMAKE_C_FLAGS${MODE}}")
        string(REPLACE "/MDd" "/MD" TMP "${CMAKE_CXX_FLAGS${MODE}}")
        set(CMAKE_CXX_FLAGS${MODE} "${TMP}" CACHE STRING "" FORCE)
        message(STATUS "CXX_${MODE}=${CMAKE_CXX_FLAGS${MODE}}")
    endforeach(MODE)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=1 -DRPP_DEBUG=1)
endif()

get_directory_property(CURRENT_DEFINES DIRECTORY ${CMAKE_SOURCE_DIR} COMPILE_DEFINITIONS)
message("ReCpp ${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS} ")
message("ReCpp ${COMPILE_DEFINITIONS}: ${CURRENT_DEFINES}")

####################################################

find_package(Threads REQUIRED)
if(LINUX)
    find_package(DWARF)
else()
    set(DWARF_LIBRARY "")
endif()

file(GLOB_RECURSE RPP_SRC rpp/*.cpp rpp/*.h rpp/*.natvis)

source_group(rpp FILES ${RPP_SRC})
add_library(ReCpp ${RPP_SRC})


target_compile_definitions(ReCpp PUBLIC RPP_TESTS_DEFINE_MAIN=0)
target_include_directories(ReCpp PUBLIC ".")
target_compile_options(ReCpp PRIVATE ${RPP_CXX_FLAGS})
target_link_libraries(ReCpp PUBLIC Threads::Threads ${DWARF_LIBRARY})
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
