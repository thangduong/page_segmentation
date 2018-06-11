macro(print_all_variables)
    message(STATUS "print_all_variables------------------------------------------{")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
    message(STATUS "print_all_variables------------------------------------------}")
endmacro()

cmake_minimum_required(VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

project(segment)
#find_package(OpenCV)
find_package(OpenCV REQUIRED PATHS "../opencv/build")
find_package(gflags REQUIRED PATHS "../gflags")
#find_package(glog REQUIRED PATHS "../glog")
#find_path(GLOG_INCLUDE_DIR "glog/logging.h" "../glog")
include_directories(${OpenCV_INCLUDE_DIRS})
#include_directories(gflags GLOG_INCLUDE_DIR)
print_all_variables()
add_executable(segment segment.cpp)
target_link_libraries(segment ${OpenCV_LIBS})
target_link_libraries(segment gflags)
#target_link_libraries(segment glog)
if (UNIX)
set_target_properties(segment PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY_DEBUG ../bin/linux64/
                      RUNTIME_OUTPUT_DIRECTORY_RELEASE ../bin/linux64/)
endif (UNIX)
if (WIN32)
set_target_properties(segment PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY_DEBUG ../bin/Win64/
                      RUNTIME_OUTPUT_DIRECTORY_RELEASE ../bin/Win64/)
endif (WIN32)
