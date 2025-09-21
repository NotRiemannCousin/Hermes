#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Hermes::Hermes" for configuration ""
set_property(TARGET Hermes::Hermes APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(Hermes::Hermes PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libHermes.a"
  )

list(APPEND _cmake_import_check_targets Hermes::Hermes )
list(APPEND _cmake_import_check_files_for_Hermes::Hermes "${_IMPORT_PREFIX}/lib/libHermes.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
