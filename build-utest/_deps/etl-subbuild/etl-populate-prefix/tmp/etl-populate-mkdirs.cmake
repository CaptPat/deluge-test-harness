# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-src")
  file(MAKE_DIRECTORY "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-src")
endif()
file(MAKE_DIRECTORY
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-build"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-subbuild/etl-populate-prefix"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-subbuild/etl-populate-prefix/tmp"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-subbuild/etl-populate-prefix/src/etl-populate-stamp"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-subbuild/etl-populate-prefix/src"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-subbuild/etl-populate-prefix/src/etl-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-subbuild/etl-populate-prefix/src/etl-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/etl-subbuild/etl-populate-prefix/src/etl-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
