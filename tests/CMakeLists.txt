CPMFindPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        GIT_TAG v1.17.0
)

include(GoogleTest)

add_executable(
        HermesTests

        Endpoint/IpAddressTests.cpp
        Endpoint/IpEndpointTests.cpp

        Socket/DefaultSocketTests.cpp
        Socket/TlsSocketTests.cpp

        Utils/DropLastTests.cpp
        Utils/UntilMatchTests.cpp
)

add_custom_command(
        TARGET HermesTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/cert.pfx"
        "$<TARGET_FILE_DIR:HermesTests>/"
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