cmake_minimum_required(VERSION 3.23)

project(threaded_rng_cache)

set(CMAKE_BUILD_TYPE Release)

add_library(${CMAKE_PROJECT_NAME} INTERFACE)

target_compile_features(${CMAKE_PROJECT_NAME}
    INTERFACE
        cxx_std_20
)

target_include_directories(${CMAKE_PROJECT_NAME}
    INTERFACE
        include
)

target_sources(${CMAKE_PROJECT_NAME}
    INTERFACE
        include/threaded_rng_cache.hpp
)

add_subdirectory(src)