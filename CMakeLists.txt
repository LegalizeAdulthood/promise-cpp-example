cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(promise-cpp-example)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(RapidJSON CONFIG REQUIRED)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(comics-client)
add_subdirectory(comics-server)
add_subdirectory(comicsdb)
add_subdirectory(test)
