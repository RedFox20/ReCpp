# mama.cmake already needs 3.28 for a module file set, and it keeps the headers below that.
# A 3.28 minimum here stops cmake before it can take the header path.
cmake_minimum_required(VERSION 3.25)
project(RppModuleConsumer C CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/mama.cmake)

add_executable(RppModuleConsumer main.cpp)
target_include_directories(RppModuleConsumer PRIVATE ${MAMA_INCLUDES})

# a C consumer must still see the RPP_*_SIZE macros from config.h
add_executable(RppConfigCVisibility c_visibility.c)
target_include_directories(RppConfigCVisibility PRIVATE ${MAMA_INCLUDES})
# adds the exported .cppm to this target, and defines MAMA_HAS_MODULES when the
# toolchain builds modules
mama_target_modules(RppModuleConsumer)
target_link_libraries(RppModuleConsumer PRIVATE ${MAMA_LIBS})
install(TARGETS RppModuleConsumer RUNTIME DESTINATION bin)
