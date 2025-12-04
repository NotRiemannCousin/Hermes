add_executable(
        Tests

        TcpTests.cpp
        IpTests.cpp
        main.cpp
)


target_include_directories(
        Tests
        PUBLIC
        ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(Tests PRIVATE Hermes)