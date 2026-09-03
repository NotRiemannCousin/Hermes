cmake_minimum_required(VERSION 3.29.1)
project(
        Hermes
        VERSION 0.6.0
        DESCRIPTION "A simple functional-based socket wrapper library"
        LANGUAGES CXX
)

#region Settings
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_DEBUG_POSTFIX "_d")

set(PROJECT_VERSION_SUFFIX "-beta")
set(PROJECT_VERSION_FULL "${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
#endregion

#region options
include(CMakeDependentOption)

option(HERMES_BUILD_TESTS    "Build Hermes tests"           ${PROJECT_IS_TOP_LEVEL})
option(HERMES_BUILD_EXAMPLES "Build Hermes examples"        ${PROJECT_IS_TOP_LEVEL})
option(HERMES_INSTALL        "Generate the install target." ${PROJECT_IS_TOP_LEVEL})
option(HERMES_BUILD_DOCS     "Build Doxygen documentation"  OFF)

option(HERMES_ENABLE_ASYNC "Enable async sockets (pulls in stdexec)" OFF)
cmake_dependent_option(HERMES_ENABLE_NATIVE_SCHEDULER
        "Enable FastIoLoop native scheduler (io_uring/IOCP)" OFF
        "HERMES_ENABLE_ASYNC" OFF)
option(HERMES_ENABLE_TLS "Enable TLS support (OpenSSL/SChannel)" ON)
#endregion

#region top-level compiler options
add_library(HermesCompilerFlags INTERFACE)
if(PROJECT_IS_TOP_LEVEL)
    if(MSVC)
        target_compile_options(HermesCompilerFlags INTERFACE
                $<IF:$<CONFIG:Release>,/permissive- /O2 /GL /Oi /Ot /GT /Gw /Gy /Zc:inline /openmp:llvm,>
        )
        target_link_options(HermesCompilerFlags INTERFACE $<IF:$<CONFIG:Release>,/LTCG,>)
    endif()
endif()
#endregion



#region extern aux files

enable_testing()
include(cmake/GetCPM.cmake)
include(cmake/Macros.cmake)
register_lldb_script(Hermes)

#endregion


add_library(Hermes)
add_library(Hermes::Hermes ALIAS Hermes)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HermesConfig.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/include/Hermes/Config.hpp"
        @ONLY
)
target_include_directories(Hermes PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/Hermes/Config.hpp"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Hermes)


set_target_properties(Hermes PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
        POSITION_INDEPENDENT_CODE            ON
)

#region Hermes Files
file(GLOB_RECURSE HERMES_PUBLIC_HEADERS CONFIGURE_DEPENDS include/Hermes/*.hpp include/Hermes/*.tpp)

target_sources(Hermes PRIVATE
        src/Hermes/_base/Network.cpp

        src/Hermes/Endpoint/IpEndpoint/IpAddress.cpp
        src/Hermes/Endpoint/IpEndpoint/IpEndpoint.cpp
        src/Hermes/_base/Credentials.cpp
        src/Hermes/_base/ConnectionErrorEnum.cpp
        PUBLIC
        FILE_SET headers
        TYPE HEADERS
        BASE_DIRS include
        FILES ${HERMES_PUBLIC_HEADERS}
)

if(HERMES_ENABLE_TLS)
    target_sources(Hermes PRIVATE src/Hermes/Socket/_base/TlsSession.cpp)
endif()
#endregion

#region linking
if(HERMES_ENABLE_ASYNC)
    CPMFindPackage(
            NAME stdexec
            VERSION 0.10.0
            GITHUB_REPOSITORY NVIDIA/stdexec
            GIT_TAG nvhpc-26.05
            OPTIONS "STDEXEC_BUILD_EXAMPLES OFF" "STDEXEC_INSTALL ${HERMES_INSTALL}"
            FIND_PACKAGE_ARGUMENTS "CONFIG"
    )
    target_link_libraries(Hermes PUBLIC STDEXEC::stdexec)
endif()

if(TARGET HermesCompilerFlags)
    target_link_libraries(Hermes PRIVATE $<BUILD_INTERFACE:HermesCompilerFlags>)
endif()

if(WIN32)
    target_link_libraries(Hermes PUBLIC ws2_32)
    if(HERMES_ENABLE_TLS)
        target_link_libraries(Hermes PUBLIC crypt32 secur32)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_link_libraries(Hermes PUBLIC stdc++exp)
    endif()
else()
    if(HERMES_ENABLE_TLS)
        find_package(OpenSSL REQUIRED)
        target_link_libraries(Hermes PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    endif()
    if(HERMES_ENABLE_NATIVE_SCHEDULER)
        target_link_libraries(Hermes PUBLIC uring)
    endif()
endif()
#endregion


#region subdirectories

add_subdirectory_if(HERMES_BUILD_TESTS    tests)
add_subdirectory_if(HERMES_BUILD_EXAMPLES examples)
add_subdirectory_if(HERMES_BUILD_DOCS     docs)

#endregion


#region install
if(HERMES_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(HERMES_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}@${PROJECT_VERSION_FULL}")

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
            DESTINATION ${HERMES_INSTALL_CMAKEDIR}
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/HermesConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY AnyNewerVersion
    )

    configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HermesConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/HermesConfig.cmake"
            INSTALL_DESTINATION ${HERMES_INSTALL_CMAKEDIR}
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/HermesConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/HermesConfigVersion.cmake"
            DESTINATION ${HERMES_INSTALL_CMAKEDIR}
    )
endif()
#endregion


generate_lldbinit()