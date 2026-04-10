add_executable(
        BasicClientExample
        BasicClient.cpp
)

add_executable(
        BasicServerExample
        BasicServer.cpp
)

add_executable(
        fujIServerExample
        fujIServer.cpp
)

target_link_libraries(BasicClientExample PRIVATE Hermes)
target_link_libraries(BasicServerExample PRIVATE Hermes)
target_link_libraries(fujIServerExample PRIVATE Hermes)
