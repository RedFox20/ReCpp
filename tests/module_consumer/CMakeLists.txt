cmake_minimum_required(VERSION 3.28) # a module file set needs this cmake version
project(RppModuleConsumer CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/mama.cmake)

add_executable(RppModuleConsumer main.cpp)
target_include_directories(RppModuleConsumer PRIVATE ${MAMA_INCLUDES})
# adds the exported .cppm to this target, and defines MAMA_HAS_MODULES when the
# toolchain builds modules
mama_target_modules(RppModuleConsumer)
target_link_libraries(RppModuleConsumer PRIVATE ${MAMA_LIBS})
install(TARGETS RppModuleConsumer RUNTIME DESTINATION bin)
