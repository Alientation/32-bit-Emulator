find_package(Git)

if(GIT_EXECUTABLE)
    get_filename_component(CMAKE_SOURCE_DIR ${SRC} DIRECTORY)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE ALIENEMULATOR_VERSION
        RESULT_VARIABLE ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Alien Emulator version: ${ALIENEMULATOR_VERSION}")
endif()

if(ALIENEMULATOR_VERSION STREQUAL "")
    set(ALIENEMULATOR_VERSION "0.0.0-unknown")
    message(WARNING WARNING "Failed to determine version from Git tags. Using default version \"${ALIENEMULATOR_VERSION}\".")
endif()

configure_file(${SRC} ${DST} @ONLY)