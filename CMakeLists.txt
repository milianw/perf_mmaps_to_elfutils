cmake_minimum_required(VERSION 3.1.0)
project(perf_mmaps_to_elfutils VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ ${CMAKE_MODULE_PATH})

include(FeatureSummary)

find_package(LibElf REQUIRED)
find_package(Elfutils REQUIRED)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

include_directories(${LIBELF_INCLUDE_DIRS} ${LIBDW_INCLUDE_DIR}/elfutils ${LIBDWARF_INCLUDE_DIRS})
add_executable(perf_mmaps_to_elfutils perf_mmaps_to_elfutils.cpp)
target_link_libraries(perf_mmaps_to_elfutils ${LIBDW_LIBRARIES})
