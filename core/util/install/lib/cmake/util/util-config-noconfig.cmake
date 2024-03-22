#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "util::util" for configuration ""
set_property(TARGET util::util APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(util::util PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libutil.a"
  )

list(APPEND _cmake_import_check_targets util::util )
list(APPEND _cmake_import_check_files_for_util::util "${_IMPORT_PREFIX}/lib/libutil.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
