# Version header generation. Runs on every build so the git version stays current; configure_file
# only rewrites the header when its contents change, so this does not force rebuilds.
set(VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(VERSION_HEADER "${VERSION_HEADER_DIR}/version.h")

add_custom_target(generate_version_header ALL
    COMMAND ${CMAKE_COMMAND} -D SRC=${PROJECT_SOURCE_DIR}/src/version.h.in
                             -D DST=${VERSION_HEADER}
                             -D GIT_WORK_DIR=${PROJECT_SOURCE_DIR}
                             -P ${PROJECT_SOURCE_DIR}/cmake/GenerateVersion.cmake
    BYPRODUCTS ${VERSION_HEADER}
    COMMENT "Generating version header"
    VERBATIM
)

add_executable(emulator_app
    src/main.cpp
)

target_include_directories(emulator_app PRIVATE ${VERSION_HEADER_DIR})

target_link_libraries(emulator_app PRIVATE
    aemu::assembler
)

add_dependencies(emulator_app generate_version_header)
aemu_target_defaults(emulator_app)

# Optional: additional executables (kernel)
# add_executable(kernel src/kernel.cpp)
# target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${VERSION_HEADER_DIR})
# target_link_libraries(kernel PRIVATE aemu::assembler)
# add_dependencies(kernel generate_version_header)
# aemu_target_defaults(kernel)
