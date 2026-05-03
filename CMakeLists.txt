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
)

add_library(Hermes
        src/Hermes/_base/Network.cpp

        src/Hermes/Endpoint/IpEndpoint/IpAddress.cpp
        src/Hermes/Endpoint/IpEndpoint/IpEndpoint.cpp
        src/Hermes/_base/Credentials.cpp
)

target_include_directories(Hermes PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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


target_link_libraries(Hermes PRIVATE stdexec)


set_target_properties(Hermes PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        POSITION_INDEPENDENT_CODE ON
)


add_subdirectory(examples)
add_subdirectory(tests)


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