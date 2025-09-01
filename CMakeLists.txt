cmake_minimum_required(VERSION 3.20)
project(socketCommunicator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimize and suppress warnings globally
add_compile_options(-O3 -w)

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

add_executable(socket_communicator_test tests/CommunicatorTest.cpp)

# Link libs
target_link_libraries(socket_communicator_test
    PRIVATE
        socket_communicator
        GTest::gtest_main
)
target_include_directories(socket_communicator_test PRIVATE
    ${GTEST_INCLUDE_DIRS}
)
set_target_properties(socket_communicator_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_TEST_DIR}
)

# macOS linker workaround
if(APPLE)
    target_link_options(socket_communicator_test PRIVATE -Wl,-ld_classic)
endif()

include(GoogleTest)
gtest_discover_tests(socket_communicator_test)


# ---- Tools ----
add_executable(latency_benchmark tools/LatencyBenchmark.cpp)
target_link_libraries(latency_benchmark PRIVATE socket_communicator)
set_target_properties(latency_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)

