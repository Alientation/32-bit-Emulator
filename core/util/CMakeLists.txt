cmake_minimum_required(VERSION 3.15)
project(util VERSION 0.0.1 LANGUAGES CXX)

add_library(util STATIC
    src/directory.cpp
    src/file.cpp
	src/logger.cpp
	src/logger_v3.cpp
	src/types.cpp
)

# Public headers
target_include_directories(util
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Enforce C++17
target_compile_features(util PUBLIC cxx_std_20)

# Optional debug postfix
set_target_properties(util PROPERTIES DEBUG_POSTFIX "d")

# Installation
include(GNUInstallDirs)
install(TARGETS util
    EXPORT util-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")

# Export config for find_package
install(EXPORT util-targets
    NAMESPACE util::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/util
)

# TODO: add util tests
# Build tests if requested
# if(BUILD_TESTS)
    # add_subdirectory(tests)
# endif()