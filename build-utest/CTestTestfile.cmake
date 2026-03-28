# CMake generated Testfile for 
# Source directory: P:/VSCodeDesktop/projects/deluge-test-harness
# Build directory: P:/VSCodeDesktop/projects/deluge-test-harness/build-utest
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[HarnessTests]=] "P:/VSCodeDesktop/projects/deluge-test-harness/build-utest/HarnessTests.exe")
set_tests_properties([=[HarnessTests]=] PROPERTIES  _BACKTRACE_TRIPLES "P:/VSCodeDesktop/projects/deluge-test-harness/CMakeLists.txt;553;add_test;P:/VSCodeDesktop/projects/deluge-test-harness/CMakeLists.txt;0;")
subdirs("_deps/cpputest-build")
subdirs("_deps/etl-build")
