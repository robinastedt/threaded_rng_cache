
add_executable(performance_test
    performance_test.cpp
)

target_link_libraries(performance_test
    PRIVATE
        threaded_rng_cache
)