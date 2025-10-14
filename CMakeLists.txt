cmake_minimum_required(VERSION 3.29.1)
project(Hermes LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_VERSION_NUMBER 0.2.0)
set(PROJECT_VERSION_SUFFIX "")

set(PROJECT_VERSION "${PROJECT_VERSION_NUMBER}-${PROJECT_VERSION_SUFFIX}")

set(CMAKE_CXX_STANDARD 23) # TODO: change to 26 when annotations arrive
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#----------------------------------------------------------------------------------------------------
#       Hermes
#----------------------------------------------------------------------------------------------------

add_library(
        Hermes

        src/Hermes/_base/Network.cpp

        src/Hermes/Endpoint/IpEndpoint/IpAddress.cpp
        src/Hermes/Endpoint/IpEndpoint/IpEndpoint.cpp
)

target_include_directories(
        Hermes
        PUBLIC

        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(Hermes PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        POSITION_INDEPENDENT_CODE ON
)


#----------------------------------------------------------------------------------------------------
#       Examples
#----------------------------------------------------------------------------------------------------
add_executable(
        HermesExample
        examples/main.cpp
)

target_link_libraries(HermesExample PRIVATE Hermes)

#----------------------------------------------------------------------------------------------------
#       Tests
#----------------------------------------------------------------------------------------------------
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
target_link_libraries(Tests PRIVATE Hermes)



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
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/HermesConfigVersion.cmake"
        VERSION ${PROJECT_VERSION_NUMBER}
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