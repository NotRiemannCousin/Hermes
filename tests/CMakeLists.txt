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
        DefaultSocketTests.cpp
        TlsSocketTests.cpp
)

add_custom_command(
        TARGET HermesTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/cert.pfx"
        "$<TARGET_FILE_DIR:HermesTests>/cert.pfx"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cert.pfx"
        COMMENT "Copying certificate for TLS tests..."
        VERBATIM
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