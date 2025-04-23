cmake_minimum_required(VERSION 3.10)

project(AdaptorPrototypeMk4)

# Set C++03 standard
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include src)

# Define WIN32_LEAN_AND_MEAN to avoid Windows.h conflicts
# Define _CRT_SECURE_NO_WARNINGS to suppress warnings about deprecated C runtime functions
add_definitions(-DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)

# Source files for the main executable
set(SOURCES
    src/main.cpp
    src/shared_memory.cpp
    src/network_sync.cpp
    src/config.cpp
    src/change_tracking.cpp
    src/memory_layout.h
    src/sync_message.h
    src/change_tracking.h
)

# Create the main executable
add_executable(AdaptorPrototypeMk4 ${SOURCES})

# Link against required libraries
target_link_libraries(AdaptorPrototypeMk4 ws2_32)

# Testing configuration
enable_testing()

# Only add the test directory if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test)
    # Find GTest package
    find_package(GTest QUIET)

    # If GTest is found, add the test subdirectory
    if(GTest_FOUND)
        add_subdirectory(test)
    else()
        message(STATUS "GTest not found, tests will not be built")
    endif()
endif()