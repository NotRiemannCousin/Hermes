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

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(
        HermesApp
        app/main.cpp
)

target_link_libraries(HermesApp PUBLIC Hermes)
