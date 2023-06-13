find_package(GTest CONFIG REQUIRED)

add_executable(test test.cpp)
target_link_libraries(test PRIVATE GTest::gmock_main)
