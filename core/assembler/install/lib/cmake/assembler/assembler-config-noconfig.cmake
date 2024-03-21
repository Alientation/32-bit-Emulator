#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "assembler::assembler" for configuration ""
set_property(TARGET assembler::assembler APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(assembler::assembler PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libassembler.a"
  )

list(APPEND _cmake_import_check_targets assembler::assembler )
list(APPEND _cmake_import_check_files_for_assembler::assembler "${_IMPORT_PREFIX}/lib/libassembler.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
