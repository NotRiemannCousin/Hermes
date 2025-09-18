cmake_minimum_required(VERSION 3.29.1)
project(Hermes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/wd4844)

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
        HermesApp
        app/main.cpp
)


add_executable(
        Tests
        tests/_base.hpp

        tests/TcpTests.hpp
        tests/IpTests.hpp
        tests/_tests.cpp
)

target_link_libraries(HermesApp PRIVATE Hermes)
target_link_libraries(Tests PRIVATE Hermes)
