cmake_minimum_required(VERSION 3.25)
project(RppConsumer)

# The exported headers use concepts, so a consumer must match the standard ReCpp
# was built with. The CI job sets CXX20=1 for both sides.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ${MAMA_INCLUDES} and ${MAMA_LIBS} come from the ReCpp package mama exported
include(mama.cmake)
include_directories(${MAMA_INCLUDES})

add_executable(RppConsumer main.cpp)
target_link_libraries(RppConsumer PRIVATE ${MAMA_LIBS})

# mama runs the install target after every build
install(TARGETS RppConsumer DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)
