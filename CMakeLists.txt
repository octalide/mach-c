cmake_minimum_required(VERSION 3.10)
project(cmach)

list(APPEND CMAKE_PREFIX_PATH "X:/bin/llvm")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set the C standard
set(CMAKE_C_STANDARD 11)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Specify the source files
set(SRC_DIR src)
set(INCLUDE_DIR include)
file(GLOB SRCS "${SRC_DIR}/*.c")

# Include directories
include_directories(${INCLUDE_DIR})
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Conditionally add debug flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building with debug symbols")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
endif()

# Add the executable
add_executable(${PROJECT_NAME} ${SRCS})

# Link against LLVM libraries
target_link_libraries(${PROJECT_NAME} ${llvm_libs})
