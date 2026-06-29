cmake_minimum_required(VERSION 3.29.1)
project(
        Hermes
        VERSION 0.5.3
        DESCRIPTION "A simple functional-based socket wrapper library"
        LANGUAGES CXX
)

set(PROJECT_VERSION_SUFFIX "-beta")
set(FULL_PROJECT_VERSION "${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_DEBUG_POSTFIX "_d")
set(CMAKE_CXX_STANDARD_REQUIRED   ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES    OFF)




option(HERMES_BUILD_TESTS    "Build Hermes tests"           ${PROJECT_IS_TOP_LEVEL})
option(HERMES_BUILD_EXAMPLES "Build Hermes examples"        ${PROJECT_IS_TOP_LEVEL})
option(HERMES_INSTALL        "Generate the install target." ${PROJECT_IS_TOP_LEVEL})


include(cmake/get_cpm.cmake)
CPMAddPackage(URI "gh:NVIDIA/stdexec#main" OPTIONS "STDEXEC_BUILD_EXAMPLES OFF")


add_library(Hermes)
add_library(Hermes::Hermes ALIAS Hermes)


file(GLOB_RECURSE HERMES_PUBLIC_HEADERS CONFIGURE_DEPENDS include/Hermes/*.hpp include/Hermes/*.tpp)

target_sources(Hermes PRIVATE
        src/Hermes/_base/Network.cpp

        src/Hermes/Endpoint/IpEndpoint/IpAddress.cpp
        src/Hermes/Endpoint/IpEndpoint/IpEndpoint.cpp
        src/Hermes/_base/Credentials.cpp
        src/Hermes/Socket/_base/TlsSession.cpp
        src/Hermes/_base/ConnectionErrorEnum.cpp
        PUBLIC
        FILE_SET headers
        TYPE HEADERS
        BASE_DIRS include
        FILES ${HERMES_PUBLIC_HEADERS}
)
set_target_properties(Hermes PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)

target_link_libraries(Hermes PUBLIC
    $<BUILD_INTERFACE:stdexec>
    $<INSTALL_INTERFACE:stdexec_EXTERNAL>
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
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
)

if(HERMES_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(HERMES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HERMES_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS Hermes
            EXPORT HermesTargets
            FILE_SET headers
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(EXPORT HermesTargets
            FILE HermesTargets.cmake
            NAMESPACE Hermes::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hermes
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/HermesConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY AnyNewerVersion
    )

    configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HermesConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/HermesConfig.cmake"
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hermes
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/HermesConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/HermesConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hermes
    )
endif()