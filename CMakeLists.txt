cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(promise-cpp-example)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CTest)

find_package(RapidJSON CONFIG REQUIRED)
find_package(promise-cpp REQUIRED)
find_package(BoostAsio REQUIRED)
find_package(BoostBeast REQUIRED)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(comics-client)
add_subdirectory(comics-server)
add_subdirectory(comicsdb)
add_subdirectory(examples)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
