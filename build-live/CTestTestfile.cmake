# CMake generated Testfile for 
# Source directory: /mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d
# Build directory: /mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/build-live
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(state_machine "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/build-live/test_state_machine")
set_tests_properties(state_machine PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;95;add_test;/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;0;")
add_test(soap "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/build-live/test_soap")
set_tests_properties(soap PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;99;add_test;/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;0;")
add_test(md5 "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/build-live/test_md5")
set_tests_properties(md5 PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;103;add_test;/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;0;")
add_test(connection_request "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/build-live/test_connection_request")
set_tests_properties(connection_request PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;107;add_test;/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;0;")
add_test(transformer "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/build-live/test_transformer")
set_tests_properties(transformer PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;113;add_test;/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;0;")
add_test(hot_reload "bash" "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/tests/test_hot_reload.sh" "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/build-live/tr69d" "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d")
set_tests_properties(hot_reload PROPERTIES  TIMEOUT "20" _BACKTRACE_TRIPLES "/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;117;add_test;/mnt/c/Users/DELL/Desktop/Mock_tr69_clone/Mock_Tr69d/CMakeLists.txt;0;")
