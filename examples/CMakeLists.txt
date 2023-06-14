# make a header-only interface library for the add ons.
add_library(promise-cpp-add-ons INTERFACE)
target_include_directories(promise-cpp-add-ons INTERFACE .)
target_link_libraries(promise-cpp-add-ons INTERFACE promise-cpp)
target_sources(promise-cpp-add-ons INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/add_ons/asio/io.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/add_ons/asio/timer.hpp
)
set_target_properties(promise-cpp-add-ons PROPERTIES FOLDER Examples)

function(add_example name)
    set(target ${counter}-${name})
    math(EXPR counter "${counter}+1")
    set(counter ${counter} PARENT_SCOPE)
    add_executable(${target} ${name}.cpp)
    target_link_libraries(${target} PRIVATE promise-cpp-add-ons boost::beast boost::asio)
    set_target_properties(${target} PROPERTIES FOLDER Examples)
endfunction()

set(counter 1)
add_example(asio_timer)
add_example(asio_http_client)
add_example(asio_http_server)
