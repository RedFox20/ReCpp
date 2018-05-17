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
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(GCC TRUE)
elseif(MSVC)
else()
    message(FATAL_ERROR "Unsupported CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

############## RPP Compilation Flags ##############
if(CLANG OR GCC)
    add_compile_options(-Wall -std=c++1z)
    if(CLANG)
        add_compile_options(-stdlib=libc++)
    endif()
    if(LINUX)
        add_compile_options(-march=native)
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
    # /MP - MultiProcess build
    add_compile_options(/std:c++latest /arch:AVX2 /Oi /MD /wd4996 /wd4819 /wd4251 /wd4275 /W3 /Zi /MP)
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
message("ReCpp ${CMAKE_BUILD_TYPE} CXX FLAGS: ${CMAKE_CXX_FLAGS} ")
message("ReCpp DEFINES: ${CURRENT_DEFINES}")
###################################################

find_package(Threads REQUIRED)
if(LINUX)
    find_package(DWARF)
    message("DWARF LIBS: ${DWARF_LIBRARY}")
else()
    set(DWARF_LIBRARY "")
endif()
if(ANDROID)
    set(RUNTIME android log)
else()
    set(RUNTIME "")
endif()

file(GLOB_RECURSE RPP_SRC rpp/*.cpp rpp/*.h rpp/*.natvis)

source_group(rpp FILES ${RPP_SRC})
add_library(ReCpp ${RPP_SRC})
set_target_properties(ReCpp PROPERTIES XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")

target_compile_definitions(ReCpp PUBLIC RPP_TESTS_DEFINE_MAIN=0)
target_include_directories(ReCpp PUBLIC ".")
target_link_libraries(ReCpp PUBLIC Threads::Threads ${DWARF_LIBRARY} ${RUNTIME})

install(TARGETS ReCpp DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)

###################################################

if (NOT APPLE)
    file(GLOB_RECURSE RPP_TESTS tests/*.cpp)
    add_executable(RppTests ${RPP_TESTS})
    set_target_properties(RppTests PROPERTIES XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")

    target_include_directories(RppTests PUBLIC ".")
    target_link_libraries(RppTests ReCpp)

    install(TARGETS RppTests DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()

###################################################


