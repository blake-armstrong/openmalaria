# Root CMake configuration for OpenMalaria

# This file is part of OpenMalaria.
# 
# Copyright (C) 2005-2025 Swiss Tropical and Public Health Institute
# Copyright (C) 2005-2015 Liverpool School Of Tropical Medicine
# Copyright (C) 2020-2025 University of Basel
# Copyright (C) 2025 The Kids Research Institute Australia
#
# OpenMalaria is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

cmake_minimum_required (VERSION 3.15)

project (OpenMalaria CXX)

# The FindXXX.cmake files included are not quite the same as standard ones.
# They abort if not found, and look in a few extra places (e.g. ${CMAKE_SOURCE_DIR}/lib).
set (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# -----  Compile options  -----

set(CMAKE_CXX_STANDARD 20)

set (OM_STD_LIBS)

if (NOT MSVC)
  # We almost always want optimisations enabled:
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2")
endif (NOT MSVC)

if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fp-model strict")
endif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")

# Statically link libgcc; isn't going to work when other C++ libraries are dynamically linked
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  option (OM_COVERAGE "Produce coverage into binary.")
  if (OM_COVERAGE)
    # Add coverage
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  endif (OM_COVERAGE)
  
  option (OM_STATICALLY_LINK "Attempt to link libraries, including stdc++, statically.")
  if (OM_STATICALLY_LINK)
    link_directories (${CMAKE_SOURCE_DIR}/lib)
    # Note: -static-libstdc++ is only available since GCC 4.5
    # See: http://www.trilithium.com/johan/2005/06/static-libstdc/
    add_definitions(-static-libgcc -static-libstdc++)
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -static-libgcc -static-libstdc++")
  endif (OM_STATICALLY_LINK)
endif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")

option (BUILD_SHARED_LIBS "Link xsdcxx, model, etc. libraries dynamically instead of statically (almost certainly not wanted)." OFF)
mark_as_advanced (BUILD_SHARED_LIBS)

if (MSVC)
  # We almost always want optimisations enabled:
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /O2")
  # This is now the OM_STATICALLY_LINK for windows
  option (OM_USE_LIBCMT "Use LIBCMT.lib instead of MSVCRT[D].lib when linking, and link statically. This removes a dependency on a .net library, but usually requires manually building dependency libraries." OFF)
  # set a temporary variable, since cache variables don't get rewritten
  if (OM_USE_LIBCMT)
    #piece of code from http://stackoverflow.com/questions/1618927/cmake-microsoft-visual-studio-and-monolithic-runtimes
    #Note: doesn't update cmake cache (but still works)
    #We statically link to reduce dependancies
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${flag_var} MATCHES "/MDd")
        string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
      endif(${flag_var} MATCHES "/MDd")
      if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
    
    set (OM_LINK_FLAGS "/NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD /NODEFAULTLIB:library")
    set (OM_COMPILE_FLAGS "/DXERCES_STATIC_LIBRARY")
  else (OM_USE_LIBCMT)
    set (OM_LINK_FLAGS "/NODEFAULTLIB:LIBCMT")
    set (OM_COMPILE_FLAGS)
  endif (OM_USE_LIBCMT)
  set (OM_COMPILE_FLAGS "${OM_COMPILE_FLAGS} /D_CRT_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS")
endif (MSVC)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  # These are actually only needed when using "clang" not "clang++" (same with
  # "gcc"), but adding here saves a user error:
  set (OM_STD_LIBS m stdc++)
endif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "AppleClang")
  # These are actually only needed when using "clang" not "clang++" (same with
  # "gcc"), but adding here saves a user error:
  set (OM_STD_LIBS m stdc++)
endif("${CMAKE_CXX_COMPILER_ID}" MATCHES "AppleClang")


# -----  Find dependencies  -----

set (CMAKE_LIBRARY_PATH /usr/lib64)

find_package (XSD)
find_package (XercesC)
find_package (GSL)
find_package (Z)
find_package (Python)

if (NOT PYTHON_EXECUTABLE)
    message (SEND_ERROR "Unable to find a Python interpreter (required).")
endif (NOT PYTHON_EXECUTABLE)

# -----  Compile-time optional features  -----
# (must come before add_subdirectory (model))

option (OM_STREAM_VALIDATOR "Compile in StreamValidator (see model/util/StreamValidator.h for usage notes)" OFF)
if (OM_STREAM_VALIDATOR)
  add_definitions (-DOM_STREAM_VALIDATOR)
endif (OM_STREAM_VALIDATOR)


# -----  Compile code  -----

add_subdirectory (contrib)
add_subdirectory (schema)
add_subdirectory (model)
# add_subdirectory (util/SchemaTranslator)
add_dependencies (model schema)

# -----  generate openMalaria  -----

# Emscripten/WASM is single-threaded and provides pthread symbols via libc; no
# separate pthread library exists to find, so skip this requirement there.
if (UNIX AND NOT EMSCRIPTEN)
  find_library (PTHREAD_LIBRARIES pthread PATHS ${CMAKE_SOURCE_DIR}/lib /usr/lib /usr/local/lib)
  if (NOT PTHREAD_LIBRARIES)
    message (SEND_ERROR "PThread library not found")
  endif (NOT PTHREAD_LIBRARIES)
endif (UNIX AND NOT EMSCRIPTEN)
mark_as_advanced (PTHREAD_LIBRARIES)

include_directories (SYSTEM
  ${XSD_INCLUDE_DIRS}
  ${XERCESC_INCLUDE_DIRS}
  ${GSL_INCLUDE_DIRS}
  ${Z_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/contrib
)
include_directories (
  ${CMAKE_SOURCE_DIR}/model ${CMAKE_BINARY_DIR}
)

add_executable (openMalaria model/main.cpp)

target_link_libraries (openMalaria
  model
  schema
  contrib
  ${GSL_LIBRARIES}
  ${XERCESC_LIBRARIES}
  ${Z_LIBRARIES}
  ${PTHREAD_LIBRARIES}
  ${OM_STD_LIBS}
)

if (MSVC)
  set_target_properties (openMalaria PROPERTIES
    LINK_FLAGS "${OM_LINK_FLAGS}"
    COMPILE_FLAGS "${OM_COMPILE_FLAGS}"
  )
endif (MSVC)

# Dependencies from outside this repository are linked dynamically. Since we
# cannot be sure deployment systems have the same versions, we copy the ones
# from the build system, and add an rpath entry to link from the current dir.
option (OM_COPY_LIBS "Copy linked library files")
if (OM_COPY_LIBS)
  set_target_properties (openMalaria PROPERTIES
    LINK_FLAGS "-Wl,-rpath,\"\$ORIGIN\"")
  foreach (lib ${GSL_LIBRARIES} ${XERCESC_LIBRARIES} ${Z_LIBRARIES})
    configure_file (${lib} ${CMAKE_BINARY_DIR} COPYONLY)
  endforeach (lib)
endif (OM_COPY_LIBS)

# add_executable (pVivax model/Pv_mod/Source.cpp)
# target_link_libraries (pVivax libpvivax)

# -----  WASM/Emscripten linker configuration  -----
#
# Configures the existing openMalaria target for Emscripten instead of adding
# a second target: the CLI's file-in & -out design is achieved through an 
# in-memory filesystem (MEMFS) rather than bound via Embind. See emscripten/README.md.

option (OM_BUILD_WASM "Configure the openMalaria target for Emscripten/WASM (CLI-replay via callMain/MEMFS)" OFF)
if (OM_BUILD_WASM)
  if (NOT OM_WASM_EMBED_DIR)
    message (SEND_ERROR "OM_BUILD_WASM requires OM_WASM_EMBED_DIR to point at the staged --embed-file directory")
  endif (NOT OM_WASM_EMBED_DIR)

  target_link_options (openMalaria PRIVATE
    # -fexceptions
    #   Must match the flag Xerces was built with. Emscripten disables C++
    #   exceptions by default, and both Xerces and OpenMalaria's own
    #   util/errors.h machinery rely on C++ exceptions propagating across
    #   this link.
    -fexceptions
    # -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createOpenMalaria
    #   Wraps the module in an async factory function returning a fresh,
    #   independent instance each call. OpenMalaria has global static state
    #   (sim, master_RNG, mon::internal::runtime, ModelOptions,
    #   InterventionManager, Genotypes, CommandLine's own static fields)
    #   that's set up once per run and never reset, so two scenario runs in
    #   one browser tab must not share an instance.
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createOpenMalaria
    # -sEXPORTED_RUNTIME_METHODS=FS,callMain
    #   The chosen alternative to Embind: FS (the MEMFS filesystem object)
    #   and callMain (invoke the existing, unmodified main(argc, argv), get
    #   its exit code back, without auto-running).
    -sEXPORTED_RUNTIME_METHODS=FS,callMain
    # -sINVOKE_RUN=0
    #   The JS wrapper needs to write the scenario XML into MEMFS before
    #   main() runs, so auto-invocation must be suppressed.
    # -sEXIT_RUNTIME=0
    #   callMain() is just a function call here, not a process exit; the JS
    #   side still needs to read files back out of FS afterward.
    # -sFORCE_FILESYSTEM=1
    #   The JS wrapper uses Module.FS.writeFile/readFile directly, which
    #   dead-code elimination would otherwise strip.
    -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1
    # -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB
    #   Scenario population sizes and run lengths vary enormously; a fixed
    #   memory cap would either waste memory or hard-crash on large runs.
    # -sSTACK_SIZE=1MB
    #   Bumped from the default: recursive XML DOM traversal and nested
    #   simulation logic have non-trivial call depth.
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=1MB
    # -sASSERTIONS=1
    #   Emscripten's runtime sanity checks and clearer failure messages.
    -sASSERTIONS=1
    # --embed-file: bakes runtime data files into the module's MEMFS data
    # segment (staged into OM_WASM_EMBED_DIR by build-model.sh beforehand),
    # so the resulting module has zero runtime network dependency.
    "--embed-file=${OM_WASM_EMBED_DIR}/scenario_current.xsd@/work/scenario_current.xsd"
    "--embed-file=${OM_WASM_EMBED_DIR}/densities.csv@/work/densities.csv"
    "--embed-file=${OM_WASM_EMBED_DIR}/autoRegressionParameters.csv@/work/autoRegressionParameters.csv"
  )
endif (OM_BUILD_WASM)

# -----  Python bindings (nanobind)  -----

option (OM_BUILD_PYTHON "Build nanobind-based Python bindings (openmalaria Python package)" OFF)
if (OM_BUILD_PYTHON)
  add_subdirectory (python)
endif (OM_BUILD_PYTHON)


# -----  OM_BOXTEST - black-box & unit testing  -----

option(OM_CXXTEST_ENABLE "Enable lower-level unittests using cxx (use 'make test' or Visual Studio build target)test" ON)
if (OM_CXXTEST_ENABLE)
  enable_testing()
  add_subdirectory (unittest)
endif (OM_CXXTEST_ENABLE)

option(OM_BOXTEST_ENABLE "Enable black-box testing of openMalaria (use 'make test' or Visual Studio build target)" ON)
option(OM_BOXTEST_BINARY "Run black-box tests with binary survey output" OFF)
if (OM_BOXTEST_ENABLE)
  enable_testing()
  add_subdirectory (test)
endif (OM_BOXTEST_ENABLE)
