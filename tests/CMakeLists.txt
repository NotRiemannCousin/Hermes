include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(
        HermesTests

        IpAddressTests.cpp
        IpEndpointTests.cpp
)

target_include_directories(
        HermesTests
        PUBLIC
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(
        HermesTests
        PRIVATE
        Hermes
        GTest::gtest_main
)

gtest_discover_tests(HermesTests)