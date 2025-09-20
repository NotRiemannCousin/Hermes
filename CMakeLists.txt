cmake_minimum_required(VERSION 3.29.1)
set(PROJECT_VERSION_NUMBER 0.1.0)
set(PROJECT_VERSION_SUFFIX "alpha.1")

project(Hermes VERSION ${PROJECT_VERSION_NUMBER} LANGUAGES CXX)

set(PROJECT_VERSION "${PROJECT_VERSION_NUMBER}-${PROJECT_VERSION_SUFFIX}")

set(CMAKE_CXX_STANDARD 23) # TODO: change to 26 when annotations arrive
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(
        Hermes

        src/Hermes/_base/Network.cpp

        src/Hermes/Endpoint/IpEndpoint/IpAddress.cpp
        src/Hermes/Endpoint/IpEndpoint/IpEndpoint.cpp

        src/Hermes/Socket/Tcp/TcpClientSocket.cpp
)

target_include_directories(Hermes
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

add_executable(
        HermesExample
        examples/main.cpp
)


add_executable(
        Tests

        tests/TcpTests.cpp
        tests/IpTests.cpp
        tests/main.cpp
)

target_include_directories(
        Tests
        PUBLIC
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(HermesExample PRIVATE Hermes)
target_link_libraries(Tests PRIVATE Hermes)
