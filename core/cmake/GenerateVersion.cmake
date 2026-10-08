# Script mode (cmake -P). Expects:
#   SRC:          version.h.in template
#   DST:          generated header path
#   GIT_WORK_DIR: directory inside the git repository
find_package(Git QUIET)

set(AEMU_VERSION "")
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
        WORKING_DIRECTORY ${GIT_WORK_DIR}
        OUTPUT_VARIABLE AEMU_VERSION
        RESULT_VARIABLE ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT ERROR_CODE EQUAL 0)
        set(AEMU_VERSION "")
    endif()
endif()

if(NOT AEMU_VERSION)
    set(AEMU_VERSION "0.0.0-unknown")
    message(WARNING "Failed to determine version from Git tags. Using default version \"${AEMU_VERSION}\".")
endif()

configure_file(${SRC} ${DST} @ONLY)
