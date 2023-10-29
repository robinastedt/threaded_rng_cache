# theaded_rng_cache

A multithreaded cache for random generators that is populated in the background greatly improving performance.

The default chunk size of 128 KiB was picked based on what performed best on my hardware. This may differ for you depending on the size of your L1 cache or other factors and be overridden as a template argument. In the case of using 16 threads this corresponds to 2.125 MiB of memory usage.

The values produced are completely deterministic, including their respective ordering, given a seed, although not the same as using the same distribution and engine directly.

# Performance results

    CPU: AMD Ryzen 7 5800X
    Baseline: 5.99744s (5.99744ns per iteration).
    RngCache: 0.885123s (0.885123ns per iteration). 6.77583x speedup.
