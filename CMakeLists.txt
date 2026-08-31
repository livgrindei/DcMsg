cmake_minimum_required(VERSION 3.14)

project(dcmsg
    VERSION 1.0.0
    DESCRIPTION "Compact, self-describing binary serialization format"
    LANGUAGES CXX
)

option(BUILD_SHARED_LIBS "Build dcmsg as a shared library" OFF)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(DCMSG_IS_TOP_LEVEL ON)
else()
    set(DCMSG_IS_TOP_LEVEL OFF)
endif()
option(DCMSG_BUILD_TESTS "Build the DcMsg test suite" ${DCMSG_IS_TOP_LEVEL})
option(DCMSG_BUILD_EXAMPLES "Build the DcMsg examples" ${DCMSG_IS_TOP_LEVEL})

include(GNUInstallDirs)

add_library(dcmsg
    src/dcmsg.cpp
    src/logger.cpp
)
add_library(dcmsg::dcmsg ALIAS dcmsg)

target_include_directories(dcmsg
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(dcmsg PUBLIC cxx_std_17)

# --- Install rules -----------------------------------------------------

install(TARGETS dcmsg
    EXPORT dcmsgTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT dcmsgTargets
    FILE dcmsgTargets.cmake
    NAMESPACE dcmsg::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dcmsg
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dcmsgConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dcmsgConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dcmsgConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dcmsg
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dcmsgConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/dcmsgConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dcmsg
)

# --- Examples --------------------------------------------------------------

if(DCMSG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# --- Tests ---------------------------------------------------------------

if(DCMSG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
