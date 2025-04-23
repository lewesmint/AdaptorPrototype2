# Test executable
file(GLOB TEST_SOURCES
    *.cpp
)

# Create the test executable
add_executable(adaptor_prototype_tests ${TEST_SOURCES})

# Include directories
target_include_directories(adaptor_prototype_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
)

# Link against the main library and GTest
target_link_libraries(adaptor_prototype_tests
    gtest
    gtest_main
    ws2_32
    pthread
)

# Add the test to CTest
add_test(NAME AdaptorPrototypeTests COMMAND adaptor_prototype_tests)
