add_executable(comics-server server.cpp)
target_link_libraries(comics-server comicsdb promise-cpp-add-ons boost::beast)
set_target_properties(comics-server PROPERTIES FOLDER Comics)
