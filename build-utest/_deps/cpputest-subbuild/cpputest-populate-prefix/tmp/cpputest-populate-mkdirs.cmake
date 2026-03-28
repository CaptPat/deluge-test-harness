# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-src")
  file(MAKE_DIRECTORY "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-src")
endif()
file(MAKE_DIRECTORY
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-build"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-subbuild/cpputest-populate-prefix"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-subbuild/cpputest-populate-prefix/tmp"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-subbuild/cpputest-populate-prefix/src/cpputest-populate-stamp"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-subbuild/cpputest-populate-prefix/src"
  "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-subbuild/cpputest-populate-prefix/src/cpputest-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-subbuild/cpputest-populate-prefix/src/cpputest-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/_deps/cpputest-subbuild/cpputest-populate-prefix/src/cpputest-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
