cmake_minimum_required(VERSION 3.15)

project(
    assembler
    VERSION 0.0.1
    LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -flto=auto")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto=auto")

add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
    src/assembler.cpp
    src/build.cpp
    src/directives.cpp
    src/instructions.cpp
    src/load_executable.cpp
    src/linker.cpp
    src/object_file.cpp
    src/static_library.cpp
    src/preprocessor.cpp
    src/tokenizer.cpp
)

include (FetchContent)
FetchContent_Declare(
    util
    SOURCE_DIR ../../util)
FetchContent_MakeAvailable(util)

FetchContent_Declare(
	emulator32bit
	SOURCE_DIR ../../emulator32bit
)
FetchContent_MakeAvailable(emulator32bit)

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

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_link_libraries(${PROJECT_NAME} PUBLIC util emulator32bit)

# this command will append "d" to the name of the debug version of
# the library - this is very helpful when installing as it ensures
# the debug and release version of library can be installed to the
# same location and will not conflict (overwrite each other)
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

# specify the target to install (calculator-static library defined above)
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