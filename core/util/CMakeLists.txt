cmake_minimum_required(VERSION 3.15)

project(
    util
    VERSION 0.0.1
    LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g --coverage")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -flto=auto")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto=auto")

add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
    src/directory.cpp
    src/file.cpp
	src/logger.cpp
	src/logger_v3.cpp
	src/types.cpp
)

include(GNUInstallDirs)

# sets the search paths for the include files after installation
# as well as during when building the library (as these may differ)
# this allows the library itself and users to #include the library headers
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# this command will append "d" to the name of the debug version of
# the library - this is very helpful when installing as it ensures
# the debug and release version of library can be installed to the
# same location and will not conflict (overwrite each other)
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

# specify the target to install (library defined above)
# set the export name <name>-config (does not need to match target name)
# also specify where the .lib file should be installed
install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-config
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    EXPORT ${PROJECT_NAME}-config
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/${PROJECT_NAME}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)