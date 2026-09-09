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

# a module-only consumer proves rpp.config exports the aliases without the header fallback
add_executable(RppConfigModuleOnly config_module_only.cpp)
target_include_directories(RppConfigModuleOnly PRIVATE ${MAMA_INCLUDES})
mama_target_modules(RppConfigModuleOnly)

# <rpp/tests.h> includes minmax.h, so only a module-only source proves that export list
add_executable(RppMinmaxModuleOnly minmax_module_only.cpp)
target_include_directories(RppMinmaxModuleOnly PRIVATE ${MAMA_INCLUDES})
mama_target_modules(RppMinmaxModuleOnly)

# <rpp/tests.h> also includes type_traits.h, source_loc.h, future_types.h and sprint.h
add_executable(RppMaskedModuleOnly masked_module_only.cpp)
target_include_directories(RppMaskedModuleOnly PRIVATE ${MAMA_INCLUDES})
mama_target_modules(RppMaskedModuleOnly)
# sprint.cpp holds the out-of-line half of rpp.sprint, so this target links the library
target_link_libraries(RppMaskedModuleOnly PRIVATE ${MAMA_LIBS})

# rpp::sort has to reach an importer of rpp.sort, without rpp.collections beside it
add_executable(RppSortModuleOnly sort_module_only.cpp)
target_include_directories(RppSortModuleOnly PRIVATE ${MAMA_INCLUDES})
mama_target_modules(RppSortModuleOnly)

# gcc-14 breaks a module whose importer included <string> first, so this target holds that shape
add_executable(RppStdStringModuleOnly std_string_module_only.cpp)
target_include_directories(RppStdStringModuleOnly PRIVATE ${MAMA_INCLUDES})
mama_target_modules(RppStdStringModuleOnly)
target_link_libraries(RppStdStringModuleOnly PRIVATE ${MAMA_LIBS})

# the tests.h macro split only pays when tests.macros.h plus rpp.tests writes a suite
add_executable(RppTestsModuleOnly tests_module_only.cpp)
target_include_directories(RppTestsModuleOnly PRIVATE ${MAMA_INCLUDES})
mama_target_modules(RppTestsModuleOnly)
# tests.cpp holds the out-of-line half of rpp.tests, so this target links the library
target_link_libraries(RppTestsModuleOnly PRIVATE ${MAMA_LIBS})
target_link_libraries(RppModuleConsumer PRIVATE ${MAMA_LIBS})
install(TARGETS RppModuleConsumer RUNTIME DESTINATION bin)
