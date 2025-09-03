cmake_minimum_required(VERSION 3.10)
project(ReCpp)

option(BUILD_TESTS "Build RppTests executable for unit testing?" OFF)
option(BUILD_WITH_LIBDW "Use libdw on linux for stack tracing" ON)
option(MSVC_NOCRTDBG "MSVC: No Debug CRT (enforce /MD or /MT)? This greatly improves performance." ON)
option(MSVC_DYNAMIC  "MSVC: Use Dynamic CRT?" ON)

# These are only for build systems that don't use `mamabuild` which supports these by default:
# -- mama build test asan ubsan coverage ReCpp
option(BUILD_WITH_MEM_SAFETY "Build Rpp and RppTests with memory safety checks" OFF)
option(BUILD_WITH_THREAD_SAFETY "Build Rpp and RppTests with thread safety checks" OFF)
option(BUILD_WITH_CODE_COVERAGE "Build with code coverage for unit tests" OFF)

# include mama dependencies for platforms that require it
if(MIPS)
    include(mama.cmake)
endif()

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

if(CXX17)
    message("ENABLE C++17")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
else()
    message("ENABLE C++20")
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clang-tidy

# Add compiler specific flags
if(CLANG OR GCC)
    add_compile_options(-g -Wall -Wextra -fPIC)
    if(CLANG)
        message("Configure for Clang++ ${CMAKE_CXX_COMPILER_VERSION}")
        add_compile_options(-stdlib=libc++)
    else()
        message("Configure for G++ ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        # -march=x86-64-v3: (close to Haswell) AVX, AVX2, BMI1, BMI2, F16C, FMA, LZCNT, MOVBE, XSAVE
        # -march=x86-64-v4: AVX512F, AVX512BW, AVX512CD, AVX512DQ, AVX512VL (generally not available on VM-s in 2023)
        # NOTE: on Alder Lake and later, AVX512 is no longer supported! So to support broken Intel CPU-s we cannot have AVX512
        add_compile_options(-march=x86-64-v3)
        message("Using -march=x86-64-v3 for backwards compatibility")
    endif()
    if(BUILD_WITH_MEM_SAFETY)
        message("Building with memory safety checks")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -fPIE -pie)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    elseif(BUILD_WITH_THREAD_SAFETY)
        message("Building with thread safety checks")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -fPIE -pie)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    endif()
    if(BUILD_WITH_CODE_COVERAGE)
        message("Building with code coverage")
        add_compile_options(--coverage)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
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
    add_compile_options(/arch:AVX2 /Oi /wd4996 /wd4819 /wd4251 /wd4275 /W3 /Zi /MP)
    # /MD - MultiThreaded Dynamic CRT
    # /MT - MultiThreaded Static CRT
    if (MSVC_DYNAMIC)
        message("Using Dynamic CRT")
        set(MSVC_CRT /MD)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    else()
        message("Using Static CRT")
        set(MSVC_CRT /MT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
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
            string(REPLACE "${MSVC_CRT}d" "${MSVC_CRT}" TMP1 "${CMAKE_C_FLAGS${MODE}}")
            string(REPLACE "${MSVC_CRT}d" "${MSVC_CRT}" TMP2 "${CMAKE_CXX_FLAGS${MODE}}")
            set(CMAKE_C_FLAGS${MODE} "${TMP1}" CACHE STRING "" FORCE)
            set(CMAKE_CXX_FLAGS${MODE} "${TMP2}" CACHE STRING "" FORCE)
            #message(STATUS "C_${MODE}=${CMAKE_C_FLAGS${MODE}}")
            #message(STATUS "CXX_${MODE}=${CMAKE_CXX_FLAGS${MODE}}")
        endforeach(MODE)
    endif()
    if(BUILD_WITH_MEM_SAFETY)
        message("Building with memory safety checks")
        # fsanitize fails with C++20 modules
        #add_compile_options(/fsanitize=address)
        #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /fsanitize=address")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug") # enable Runtime Checks in Debug mode
            add_compile_options(/RTC1)
        endif()
    endif()
    if(BUILD_WITH_CODE_COVERAGE)
        message(WARNING "Code coverage not supported on MSVC")
    endif()
endif()


###################################################


# Configure platform specific system dependencies
find_package(Threads REQUIRED)

set(LIBDW_LIB "")
set(LIBDL_LIB "")
if(RASPI)
    set(LIBDL_LIB "dl")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif(MIPS)
    set(LIBDL_LIB "dl")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    if (BUILD_WITH_LIBDW)
        # dependencies managed by mamabuild
        set(LIBDW_LIB ${elfutils_LIB})
        set(LIBDW_INCLUDE ${elfutils_INCLUDES})
    endif()
elseif(OCLEA)
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--as-needed")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed")
    if (BUILD_WITH_LIBDW)
        # TODO: handle differences between build systems
        set(FAKEROOT_DIR "/build/platforms/cv25/fakeroot")
        set(LIBDW_LIB "${FAKEROOT_DIR}/usr/lib/libdw.so;${FAKEROOT_DIR}/usr/lib/libelf.so")
        set(LIBDW_INCLUDE "${FAKEROOT_DIR}/usr/include")
    endif()
    set(LIBDL_LIB "dl")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif(LINUX AND BUILD_WITH_LIBDW)
    find_package(DWARF) # sets DWARF_LIBRARY and DWARF_INCLUDE_DIR
    set(LIBDW_LIB ${DWARF_LIBRARY})
    set(LIBDW_INCLUDE ${DWARF_INCLUDE_DIR})
elseif(LINUX)
    set(LIBDL_LIB "dl")
endif()

if(LIBDW_LIB)
    add_definitions(-DRPP_LIBDW_ENABLED=1)
    set(TRACE_LIBRARY ${LIBDW_LIB} ${LIBDL_LIB})
    message("RPP_LIBDW_ENABLED=1")
    message("LIBDW_LIB: ${TRACE_LIBRARY}")
    message("LIBDW_INCLUDE: ${LIBDW_INCLUDE}")
    if(LIBDW_INCLUDE)
        include_directories(${LIBDW_INCLUDE})
    endif()
elseif(LIBDL_LIB)
    set(TRACE_LIBRARY ${LIBDL_LIB})
    message("DISABLED libdw: rpp::stack_trace will not give good traces")
else()
    # windows: probably has good traces anyways
endif()

if(ANDROID)
    set(RPP_RUNTIME android log)
elseif(MIPS)
    set(RPP_RUNTIME rt atomic)
elseif(LINUX)
    set(RPP_RUNTIME rt)
elseif(APPLE)
    set(RPP_RUNTIME "-framework Foundation")
endif()

###################################################
# Echo the final flags for people who care:
get_directory_property(CURRENT_OPTIONS DIRECTORY ${CMAKE_SOURCE_DIR} COMPILE_OPTIONS)
get_directory_property(CURRENT_DEFINES DIRECTORY ${CMAKE_SOURCE_DIR} COMPILE_DEFINITIONS)
message("ReCpp ${CMAKE_BUILD_TYPE} CXXFLAGS: ${CMAKE_CXX_FLAGS}   ${CMAKE_CXX_FLAGS_${CURRENT_BUILD_TYPE}}")
message("ReCpp ${CMAKE_BUILD_TYPE} OPTIONS:  ${CURRENT_OPTIONS}")
message("ReCpp ${CMAKE_BUILD_TYPE} DEFINES:  ${CURRENT_DEFINES}")
###################################################

# gather source files
set(SRC_PATTERN src/rpp/*.cpp src/rpp/*.h src/rpp/*.natvis)
if(APPLE)
    set(SRC_PATTERN ${SRC_PATTERN} src/rpp/*.mm)
endif()
file(GLOB_RECURSE RPP_SRC ${SRC_PATTERN})
source_group(rpp FILES ${RPP_SRC})

# rpp static library
add_library(ReCpp ${RPP_SRC})
set_target_properties(ReCpp PROPERTIES XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")
target_compile_definitions(ReCpp PUBLIC RPP_TESTS_DEFINE_MAIN=0)
target_include_directories(ReCpp PUBLIC "src")
target_link_libraries(ReCpp
    PUBLIC ${RPP_RUNTIME}
    PUBLIC ${TRACE_LIBRARY}
    PUBLIC Threads::Threads
)
install(TARGETS ReCpp DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)

###################################################

if (BUILD_TESTS)
    file(GLOB_RECURSE RPP_TESTS tests/*.cpp)
    source_group(tests FILES ${RPP_TESTS})

    # we build Rpp from source again so that RPP_DEBUG=1 has an effect in unit tests
    add_executable(RppTests ${RPP_TESTS} ${RPP_SRC})
    set_target_properties(RppTests PROPERTIES XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")
    target_compile_definitions(RppTests PRIVATE -DRPP_DEBUG=1)
    target_include_directories(RppTests PUBLIC "src")
    target_link_libraries(RppTests
        ${RPP_RUNTIME}
        ${TRACE_LIBRARY}
        Threads::Threads
    )
    install(TARGETS RppTests DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()

###################################################


