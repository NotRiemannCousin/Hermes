cmake_minimum_required(VERSION 3.29.1)
project(
        Hermes
        VERSION 0.4
        DESCRIPTION "A simple functional-based socket wrapper library"
        LANGUAGES CXX
)

set(PROJECT_VERSION_SUFFIX "")
set(FULL_PROJECT_VERSION "${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_DEBUG_POSTFIX "_d")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#----------------------------------------------------------------------------------------------------
#       Hermes
#----------------------------------------------------------------------------------------------------

include(cmake/get_cpm.cmake)

# TODO: Remove when it arives at STL.
CPMFindPackage(
        NAME stdexec
        GITHUB_REPOSITORY NVIDIA/stdexec
        GIT_TAG main
        OPTIONS
        "STDEXEC_BUILD_EXAMPLES OFF"
        "STDEXEC_BUILD_TESTS OFF"
)

add_library(Hermes
        src/Hermes/_base/Network.cpp

        src/Hermes/Endpoint/IpEndpoint/IpAddress.cpp
        src/Hermes/Endpoint/IpEndpoint/IpEndpoint.cpp
        src/Hermes/_base/Credentials.cpp
        src/Hermes/Socket/_base/TlsSession.cpp
        src/Hermes/_base/ConnectionErrorEnum.cpp
)

target_include_directories(Hermes PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${stdexec_SOURCE_DIR}/include>
)


target_compile_options(Hermes PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:
        /permissive- /GL /arch:AVX2 /GT /Gw /Gy /Zc:inline /openmp:llvm
        >

        $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:Release>>:
        -Wall -Wextra -pedantic
        -flto
        -mavx2
        -fopenmp
        -ffunction-sections
        -fdata-sections
        >
)

target_link_options(Hermes PRIVATE
        $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:Release>>:
        -flto

        $<$<PLATFORM_ID:Darwin>:-Wl,-dead_strip>
        $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--gc-sections>
        >
)


target_link_libraries(Hermes PUBLIC
        $<BUILD_INTERFACE:stdexec>
)
if(WIN32)
    target_link_libraries(Hermes PUBLIC ws2_32 crypt32 secur32)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_link_libraries(Hermes PUBLIC stdc++exp)
    endif()
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(Hermes PUBLIC OpenSSL::SSL OpenSSL::Crypto uring)
endif()


set_target_properties(Hermes PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        POSITION_INDEPENDENT_CODE ON
)


if(PROJECT_IS_TOP_LEVEL)
    set(DEFAULT_BUILD_OPTS ON)
else()
    set(DEFAULT_BUILD_OPTS OFF)
endif()

option(HERMES_BUILD_EXAMPLES "Build project examples" ${DEFAULT_BUILD_OPTS})
option(HERMES_BUILD_TESTS "Build project tests" ${DEFAULT_BUILD_OPTS})

if(HERMES_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(HERMES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS Hermes
        EXPORT HermesTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        CONFIGURATIONS Debug Release
)


install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)

install(EXPORT HermesTargets
        FILE HermesTargets.cmake
        NAMESPACE Hermes::
        DESTINATION lib/cmake/Hermes
        CONFIGURATIONS Debug Release
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/HermesConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HermesConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/HermesConfig.cmake"
        INSTALL_DESTINATION lib/cmake/Hermes
)


install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/HermesConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/HermesConfigVersion.cmake"
        DESTINATION lib/cmake/Hermes
)