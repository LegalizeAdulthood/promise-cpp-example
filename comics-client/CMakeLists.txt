find_package(promise-cpp REQUIRED)
find_package(BoostBeast REQUIRED)

add_executable(comics-client client.cpp)
target_link_libraries(comics-client comicsdb promise-cpp boost::beast)
