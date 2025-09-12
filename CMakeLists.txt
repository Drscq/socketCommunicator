cmake_minimum_required(VERSION 3.20)
project(socketCommunicator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimize and suppress warnings globally
add_compile_options(-O3 -w -march=native)

# Add x86-specific SIMD flags only when building on x86/x64. These flags are
# not recognized on arm64 (e.g., Apple Silicon) and will break the build.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
    add_compile_options(-maes -msse4.1 -mavx -mavx2)
endif()

# Adopt NEW behavior for CMP0135 (URL download timestamps)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()


## Output Directories (put all artifacts under build/test)
set(OUTPUT_TEST_DIR ${CMAKE_BINARY_DIR}/test)
file(MAKE_DIRECTORY ${OUTPUT_TEST_DIR})
# Find ZeroMQ
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# Library target for core code
add_library(socket_communicator
    src/lib/Communicator.cpp
)

target_include_directories(socket_communicator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(socket_communicator
    ${ZMQ_LIBRARIES}
)

target_compile_options(socket_communicator PRIVATE
    ${ZMQ_CFLAGS_OTHER}
)

# ---- Tests ----
include(FetchContent)

FetchContent_Declare(
    googletest
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

####################################
############### Tests ##############
####################################
# Keep a simple macro to define a gtest executable and register it with CTest.
# Use a custom name to avoid clashing with CTest's built-in add_test command.
include(GoogleTest)
set(ALL_TEST_TARGETS)
macro(add_sc_test TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_TEST_DIR})
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
    )
    target_link_libraries(${TARGET_NAME} PRIVATE
        socket_communicator
        GTest::gtest_main
    )
    if(APPLE)
        target_link_options(${TARGET_NAME} PRIVATE -Wl,-ld_classic)
    endif()
    # Register tests in this binary with CTest
    gtest_discover_tests(${TARGET_NAME})
    # Track for aggregate build target
    list(APPEND ALL_TEST_TARGETS ${TARGET_NAME})
endmacro()

# Define individual test binaries per suite using the macro (always add; build by target)
add_sc_test(test_communicator tests/CommunicatorTest.cpp)
add_sc_test(test_mpc          tests/MPCPartiesTest.cpp)

# Aggregate target to build all test executables
add_custom_target(build_tests DEPENDS ${ALL_TEST_TARGETS})


# ---- Tools ----
add_executable(latency_benchmark tools/LatencyBenchmark.cpp)
target_link_libraries(latency_benchmark PRIVATE socket_communicator)
set_target_properties(latency_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)
