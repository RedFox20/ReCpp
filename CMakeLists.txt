cmake_minimum_required(VERSION 3.0)
project(ReCpp)

option(BUILD_TESTS "Build RppTests executable for unit testing?" ON)
option(MSVC_NOCRTDBG "MSVC: No Debug CRT (enforce /MD or /MT)? This greatly improves performance." ON)
option(MSVC_DYNAMIC  "MSVC: Use Dynamic CRT?" ON)

if(ANDROID OR ANDROID_NDK)
elseif(WIN32 OR MSVC)
elseif(APPLE)
elseif(UNIX)
    message("Detected UNIX system")
    set(LINUX TRUE)
else()
    message(FATAL_ERROR "Unsupported Platform: ReCpp/CMakeLists.txt needs updating!")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message("Detected Clang++")
    set(CLANG TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message("Detected G++")
    set(GCC TRUE)
elseif(MSVC)
else()
    message(FATAL_ERROR "Unsupported CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

############## RPP Compilation Flags ##############

# Configure build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE ${CMAKE_CONFIGURATION_TYPES})
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=1 -DRPP_DEBUG=1)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()
string(TOUPPER ${CMAKE_BUILD_TYPE} CURRENT_BUILD_TYPE)

# Add compiler specific flags
if(CLANG OR GCC)
    add_compile_options(-Wall)
    if(CLANG)
        message("Configure for Clang++ ${CMAKE_CXX_COMPILER_VERSION}")
        add_compile_options(-stdlib=libc++)
    else()
        message("Configure for G++ ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    if(LINUX)
        add_compile_options(-march=native)
        if(CLANG)
            if(CMAKE_CXX_COMPILER_VERSION STRLESS "5.0")
                add_compile_options(-std=c++1z)
            else()
                add_compile_options(-std=c++17)
            endif()
        elseif(GCC)
            add_compile_options(-std=c++17)
        else()
            add_compile_options(-std=c++17)
        endif()
    else()
        add_compile_options(-std=c++1z)
    endif()
elseif(MSVC)
    message("Configure for VisualC++")
    # /wd4996 - Disable deprecation warning
    # /wd4819 - Disable code page warning (OpenCV)
    # /wd4251 - Disable dll-interface warning
    # /wd4275 - Disable dll-interface base class warning
    # /W3 - warning level 3
    # /Zi - Debug Database
    # /Oi - Intrinsics Enabled
    # /MP - MultiProcess build
    add_compile_options(/std:c++latest /arch:AVX2 /Oi /wd4996 /wd4819 /wd4251 /wd4275 /W3 /Zi /MP)
    # /MD - MultiThreaded Dynamic CRT
    # /MT - MultiThreaded Static CRT
    if (MSVC_DYNAMIC)
        message("Using Dynamic CRT")
        set(MSVC_CRT /MD)
    else()
        message("Using Static CRT")
        set(MSVC_CRT /MT)
        # switch from dynamic to static linking of msvcrt
        foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif(${flag_var} MATCHES "/MD")
        endforeach(flag_var)
    endif()
    if(MSVC_NOCRTDBG)
        message("Disabling Debug CRT")
        add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
        foreach(MODE "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
            string(REPLACE "${MSVC_CRT}d" "${MSVC_CRT}" TMP "${CMAKE_C_FLAGS${MODE}}")
            string(REPLACE "${MSVC_CRT}d" "${MSVC_CRT}" TMP "${CMAKE_CXX_FLAGS${MODE}}")
            set(CMAKE_C_FLAGS${MODE} "${TMP}" CACHE STRING "" FORCE)
            set(CMAKE_CXX_FLAGS${MODE} "${TMP}" CACHE STRING "" FORCE)
            #message(STATUS "C_${MODE}=${CMAKE_C_FLAGS${MODE}}")
            #message(STATUS "CXX_${MODE}=${CMAKE_CXX_FLAGS${MODE}}")
        endforeach(MODE)
    endif()
endif()

# Echo the final flags for people who care:
get_directory_property(CURRENT_OPTIONS DIRECTORY ${CMAKE_SOURCE_DIR} COMPILE_OPTIONS)
get_directory_property(CURRENT_DEFINES DIRECTORY ${CMAKE_SOURCE_DIR} COMPILE_DEFINITIONS)
message("ReCpp ${CMAKE_BUILD_TYPE} CXXFLAGS: ${CMAKE_CXX_FLAGS}   ${CMAKE_CXX_FLAGS_${CURRENT_BUILD_TYPE}}")
message("ReCpp ${CMAKE_BUILD_TYPE} OPTIONS:  ${CURRENT_OPTIONS}")
message("ReCpp ${CMAKE_BUILD_TYPE} DEFINES:  ${CURRENT_DEFINES}")


###################################################


# Configure platform specific system dependencies
find_package(Threads REQUIRED)
if(LINUX)
    find_package(DWARF)
    message("DWARF: ${DWARF_LIBRARY}")
else()
    set(DWARF_LIBRARY "")
endif()
if(ANDROID)
    set(RPP_RUNTIME android log)
elseif(LINUX)
endif()


file(GLOB_RECURSE RPP_SRC rpp/*.cpp rpp/*.h rpp/*.natvis)

source_group(rpp FILES ${RPP_SRC})
add_library(ReCpp ${RPP_SRC})
set_target_properties(ReCpp PROPERTIES XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")

target_compile_definitions(ReCpp PUBLIC RPP_TESTS_DEFINE_MAIN=0)
target_include_directories(ReCpp PUBLIC ".")
target_link_libraries(ReCpp
    PUBLIC ${DWARF_LIBRARY}
    PUBLIC ${RPP_RUNTIME}
    PUBLIC Threads::Threads
)

install(TARGETS ReCpp DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)

###################################################

if (BUILD_TESTS AND NOT APPLE)
    file(GLOB_RECURSE RPP_TESTS tests/*.cpp)
    add_executable(RppTests ${RPP_TESTS})
    set_target_properties(RppTests PROPERTIES XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")

    target_include_directories(RppTests PUBLIC ".")
    target_link_libraries(RppTests ReCpp ${RPP_RUNTIME})

    install(TARGETS RppTests DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()

###################################################


