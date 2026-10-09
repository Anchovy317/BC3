# CMake generated Testfile for 
# Source directory: /home/Anchovy/BC3/bc3_visor_qt
# Build directory: /home/Anchovy/BC3/bc3_visor_qt/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(arranque_ayuda "/home/Anchovy/BC3/bc3_visor_qt/build/bc3_visor" "--help")
set_tests_properties(arranque_ayuda PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/Anchovy/BC3/bc3_visor_qt/CMakeLists.txt;29;add_test;/home/Anchovy/BC3/bc3_visor_qt/CMakeLists.txt;0;")
