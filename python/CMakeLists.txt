# CMake configuration for the openmalaria Python bindings (nanobind).
#
# Included via add_subdirectory(python) from the root CMakeLists.txt when
# -DOM_BUILD_PYTHON=ON, or by scikit-build-core when building/installing the
# python/ package (pyproject.toml points cmake.source-dir at the repo root
# and sets OM_BUILD_PYTHON=ON via cmake.define). Either way this file is
# processed as a subdirectory of the root CMakeLists.txt, so the `model`,
# `schema`, `contrib` targets and the GSL_LIBRARIES/XERCESC_LIBRARIES/
# Z_LIBRARIES/PTHREAD_LIBRARIES variables it needs already exist.

# The root CMakeLists.txt puts this repo's own cmake/ dir (which only has a
# FindPython.cmake that locates an interpreter, not Development components)
# on CMAKE_MODULE_PATH. That would shadow CMake's own bundled FindPython
# and break nanobind's Python discovery below. Variable assignment inside
# add_subdirectory() is scoped to this file, so this doesn't affect the
# parent (or the openMalaria CLI target it configures).
list (REMOVE_ITEM CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package (Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)

execute_process (
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE NB_DIR
)
list (APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
find_package (nanobind CONFIG REQUIRED)

nanobind_add_module (_openmalaria src/bindings.cpp)

# Belt-and-braces: the root CMakeLists.txt's directory-scoped
# include_directories() (for model/, ${CMAKE_BINARY_DIR}, and the
# XSD/XercesC/GSL/Z SYSTEM includes) already applies here since it runs
# before add_subdirectory(python). This just makes the dependency explicit.
target_include_directories (_openmalaria PRIVATE
  ${CMAKE_SOURCE_DIR}/model
  ${CMAKE_BINARY_DIR}
)

target_link_libraries (_openmalaria PRIVATE
  model
  schema
  contrib
  ${GSL_LIBRARIES}
  ${XERCESC_LIBRARIES}
  ${Z_LIBRARIES}
  ${PTHREAD_LIBRARIES}
)

install (TARGETS _openmalaria LIBRARY DESTINATION openmalaria)
