add_executable(AsyncCoroClientExample Client/main.cpp Client/AsyncCoro.cpp)
add_executable(AsyncExecClientExample Client/main.cpp Client/AsyncExec.cpp)
add_executable(BasicClientExample     Client/main.cpp Client/Basic.cpp)
add_executable(RangeClientExample     Client/main.cpp Client/Range.cpp)

add_executable(AsyncServerExample Server/AsyncServer.cpp)
add_executable(BasicServerExample Server/BasicServer.cpp)
add_executable(fujIServerExample  Server/fujIServer.cpp)

target_link_libraries(AsyncServerExample PRIVATE Hermes HermesCompilerFlags)
target_link_libraries(BasicServerExample PRIVATE Hermes HermesCompilerFlags)
target_link_libraries(fujIServerExample  PRIVATE Hermes HermesCompilerFlags)

target_link_libraries(AsyncCoroClientExample PRIVATE Hermes HermesCompilerFlags)
target_link_libraries(AsyncExecClientExample PRIVATE Hermes HermesCompilerFlags)
target_link_libraries(BasicClientExample     PRIVATE Hermes HermesCompilerFlags)
target_link_libraries(RangeClientExample     PRIVATE Hermes HermesCompilerFlags)