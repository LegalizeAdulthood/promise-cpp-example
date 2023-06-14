add_executable(comics-test test.cpp)
target_link_libraries(comics-test PRIVATE GTest::gmock_main)
set_target_properties(comics-test PROPERTIES FOLDER Tests)

add_test(NAME comics-test COMMAND comics-test)
