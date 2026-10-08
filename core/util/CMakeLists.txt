add_library(util STATIC
    src/directory.cpp
    src/file.cpp
    src/logger.cpp
    src/types.cpp
)
add_library(aemu::util ALIAS util)

# Public headers
target_include_directories(util
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(util PUBLIC cxx_std_20)
aemu_target_defaults(util)

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
