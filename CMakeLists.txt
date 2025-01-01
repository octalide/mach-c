cmake_minimum_required(VERSION 3.10)
project(cmach)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

set(SRC_DIR src)
set(INCLUDE_DIR include)
file(GLOB SRCS "${SRC_DIR}/*.c")

include_directories(${INCLUDE_DIR})
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building with debug symbols")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
endif()

add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME} ${llvm_libs})
