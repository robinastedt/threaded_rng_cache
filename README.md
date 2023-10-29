# theaded_rng_cache

A multithreaded cache for random generators that is populated in the background greatly improving performance

# Performance results

    CPU: AMD Ryzen 7 5800X
    Baseline: 5.99744s (5.99744ns per iteration).
    RngCache: 0.885123s (0.885123ns per iteration). 6.77583x speedup.
