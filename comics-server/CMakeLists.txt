find_package(promise-cpp REQUIRED)
find_package(BoostBeast REQUIRED)

add_executable(comics-server server.cpp)
target_link_libraries(comics-server comicsdb promise-cpp boost::beast)
