find_package(Git)

if(GIT_EXECUTABLE)
    get_filename_component(CMAKE_SOURCE_DIR ${SRC} DIRECTORY)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE AEMU_VERSION
        RESULT_VARIABLE ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "AEMU version: ${AEMU_VERSION}")
endif()

if(AEMU_VERSION STREQUAL "")
    set(AEMU_VERSION "0.0.0-unknown")
    message(WARNING WARNING "Failed to determine version from Git tags. Using default version \"${AEMU_VERSION}\".")
endif()

configure_file(${SRC} ${DST} @ONLY)