if(HERMES_ENABLE_TLS)
    add_example(BasicClientExample Client/main.cpp Client/Basic.cpp)
    add_example(RangeClientExample Client/main.cpp Client/Range.cpp)
endif()

add_example(BasicServerExample Server/BasicServer.cpp)

if(HERMES_ENABLE_ASYNC AND HERMES_ENABLE_NATIVE_SCHEDULER)
    if(HERMES_ENABLE_TLS)
        add_example(AsyncCoroClientExample Client/main.cpp Client/AsyncCoro.cpp)
        add_example(AsyncExecClientExample Client/main.cpp Client/AsyncExec.cpp)
    endif()

    add_example(AsyncServerExample Server/AsyncServer.cpp)
    add_example(fujIServerExample Server/fujIServer.cpp)
endif()
