add_executable(comics-client client.cpp)
target_link_libraries(comics-client comicsdb promise-cpp-add-ons boost::beast)
set_target_properties(comics-client PROPERTIES FOLDER Comics)
