# Builds ReCpp and its bare-metal tests for a Cortex-M4F on FreeRTOS, for qemu-system-arm -M mps2-an386.
# Point FETCHCONTENT_SOURCE_DIR_FREERTOS or FETCHCONTENT_SOURCE_DIR_CMSIS at a local copy to skip the download
cmake_minimum_required(VERSION 3.24)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(rpp_cortex_m C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON) # newlib declares alloca() only for gnu++, as in an STM32CubeIDE project
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# SOURCE_SUBDIR none: use the sources, and skip the CMakeLists.txt of each project
include(FetchContent)
FetchContent_Declare(freertos GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
                              GIT_TAG V11.1.0 GIT_SHALLOW TRUE SOURCE_SUBDIR none)
FetchContent_Declare(cmsis GIT_REPOSITORY https://github.com/ARM-software/CMSIS_6.git
                           GIT_TAG v6.1.0 GIT_SHALLOW TRUE SOURCE_SUBDIR none)
FetchContent_MakeAvailable(freertos cmsis)

set(RPP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CM4_FLAGS -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_compile_options(${CM4_FLAGS} -ffunction-sections -fdata-sections)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${freertos_SOURCE_DIR}/include
                    ${freertos_SOURCE_DIR}/portable/GCC/ARM_CM4F ${cmsis_SOURCE_DIR}/CMSIS/Core/Include)

add_library(freertos STATIC ${freertos_SOURCE_DIR}/tasks.c ${freertos_SOURCE_DIR}/list.c
            ${freertos_SOURCE_DIR}/queue.c ${freertos_SOURCE_DIR}/portable/GCC/ARM_CM4F/port.c
            ${freertos_SOURCE_DIR}/portable/MemMang/heap_3.c)

# every source builds, except the ones which need an OS: a new file joins the Cortex-M build at once
file(GLOB RPP_SOURCES ${RPP_ROOT}/src/rpp/*.cpp)
foreach(host_only event_loop file_io jni_cpp paths proc_utils sockets stack_trace thread_pool)
    list(REMOVE_ITEM RPP_SOURCES ${RPP_ROOT}/src/rpp/${host_only}.cpp)
endforeach()

# the test files which need no std::thread, no file system and no sockets
set(RPP_TESTS binary_stream bitutils collections minmax scope_guard serialization sort strview vec)
list(TRANSFORM RPP_TESTS PREPEND ${RPP_ROOT}/tests/test_)
list(TRANSFORM RPP_TESTS APPEND .cpp)

add_executable(RppCortexM startup.c main.cpp test_cortex_m.cpp ${RPP_TESTS} ${RPP_SOURCES})
target_include_directories(RppCortexM PRIVATE ${RPP_ROOT}/src)
target_compile_definitions(RppCortexM PRIVATE RPP_FREERTOS=1 RPP_CORTEX_M_ARCH=1
                           RPP_CORTEX_M_CORE_H="cmsdk_cm4.h" RPP_CORTEX_M_DEVICE_H="cmsdk_cm4.h")
target_link_libraries(RppCortexM PRIVATE freertos)
set_target_properties(RppCortexM PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mps2_an386.ld)
target_link_options(RppCortexM PRIVATE ${CM4_FLAGS} --specs=rdimon.specs -T${CMAKE_CURRENT_SOURCE_DIR}/mps2_an386.ld
                    -Wl,--gc-sections)

add_custom_target(run_tests
    COMMAND qemu-system-arm -M mps2-an386 -nographic -monitor none -serial none
            -semihosting-config enable=on,target=native -kernel $<TARGET_FILE:RppCortexM>
    DEPENDS RppCortexM USES_TERMINAL)
