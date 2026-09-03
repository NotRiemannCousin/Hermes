CPMFindPackage(
        NAME              GTest
        VERSION           1.17.0
        GITHUB_REPOSITORY google/googletest
        GIT_TAG           v1.17.0
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
#        Socket/FastIoLoopTests.cpp
)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cert.pfx")
    find_program(OPENSSL_EXECUTABLE openssl REQUIRED)

    add_custom_command(
            OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/cert.pfx"
            COMMAND ${OPENSSL_EXECUTABLE} req -x509 -newkey rsa:2048 -nodes
            -keyout "${CMAKE_CURRENT_BINARY_DIR}/test-key.pem"
            -out "${CMAKE_CURRENT_BINARY_DIR}/test-cert.pem"
            -days 3650 -subj "/CN=localhost"
            COMMAND ${OPENSSL_EXECUTABLE} pkcs12 -export
            -out "${CMAKE_CURRENT_SOURCE_DIR}/cert.pfx"
            -inkey "${CMAKE_CURRENT_BINARY_DIR}/test-key.pem"
            -in "${CMAKE_CURRENT_BINARY_DIR}/test-cert.pem"
            -passout pass:
            COMMENT "Generating TLS certificates"
            VERBATIM
    )
    add_custom_target(GenerateTestCert DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cert.pfx")
    add_dependencies(HermesTests GenerateTestCert)
endif()

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
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
        HermesTests
        PRIVATE
        Hermes
        GTest::gtest_main
        HermesCompilerFlags
)

gtest_discover_tests(HermesTests WORKING_DIRECTORY "$<TARGET_FILE_DIR:HermesTests>")