cmake_minimum_required(VERSION 3.15)
project(util VERSION 0.0.1 LANGUAGES CXX)

add_library(util STATIC
    src/directory.cpp
    src/file.cpp
    src/logger.cpp
    src/types.cpp
)

# Public headers
target_include_directories(util
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(util PUBLIC cxx_std_20)

target_compile_options(util PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g --coverage>
    $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
    $<$<CONFIG:Release>:-O3 -flto=auto>
)

target_link_options(util PRIVATE
    $<$<CONFIG:Debug>:--coverage>
    $<$<CONFIG:RelWithDebInfo>:-flto=auto>
    $<$<CONFIG:Release>:-flto=auto>
)

set_target_properties(util PROPERTIES DEBUG_POSTFIX "d")

include(GNUInstallDirs)
install(TARGETS util
    EXPORT util-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")

install(EXPORT util-targets
    NAMESPACE util::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/util
)

# if(BUILD_TESTS)
#     add_subdirectory(tests)
# endif()