include(FindPackageHandleStandardArgs)

if(TARGET promise-cpp)
    return()
endif()

find_path(PROMISE_CPP_INCLUDE_DIRS "promise-cpp/promise.hpp")
if(PROMISE_CPP_INCLUDE_DIRS)
    add_library(promise-cpp INTERFACE)
    target_include_directories(promise-cpp INTERFACE ${PROMISE_CPP_INCLUDE_DIRS})
    target_compile_definitions(promise-cpp INTERFACE PROMISE_HEADERONLY)
endif()

find_package_handle_standard_args(promise-cpp
    REQUIRED_VARS PROMISE_CPP_INCLUDE_DIRS)
