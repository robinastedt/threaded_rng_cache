
#include <threaded_rng_cache.hpp>

#include <iostream>
#include <chrono>
#include <string>

using Distribution = std::uniform_real_distribution<double>;
using Results = std::vector<Distribution::result_type>;

namespace {
    void touchResults(const Results& results) {
        Distribution::result_type sum = 0.0;
        for (auto value : results) {
            sum += value;
        }
        std::cout << "Produced sum: " << sum << std::endl;
    }

    class Timer {
    public:
        Timer(std::string name, size_t elements, double* result)
        : m_name(std::move(name))
        , m_elements(elements)
        , m_result(result)
        , m_baseline()
        , m_begin(std::chrono::steady_clock::now())
        {}

        Timer(std::string name, size_t elements, double baseline)
        : m_name(std::move(name))
        , m_elements(elements)
        , m_result(nullptr)
        , m_baseline(baseline)
        , m_begin(std::chrono::steady_clock::now())
        {}

        ~Timer() {
            const auto end = std::chrono::steady_clock::now();
            const std::chrono::duration<double> duration = end - m_begin;
            const std::chrono::duration<double, std::nano> durationPerElement = duration / m_elements;
            std::cout << m_name << ": "
                      << duration 
                      << " (" << durationPerElement << " per iteration).";
            if (m_baseline) {
                const double ratio = *m_baseline / duration.count();
                std::cout << " " << ratio << "x speedup.";
            }
            std::cout << std::endl;
            if (m_result) {
                *m_result = duration.count();
            }
        }

    private:
        std::string m_name;
        size_t m_elements;
        double* m_result;
        std::optional<double> m_baseline;
        std::chrono::steady_clock::time_point m_begin;
    };
}

int main() {

    const Distribution commonDistribution{0.0, 1.0};
    const size_t iterations = 1'000'000'000;

    double baselineResult = 0.0;

    {
        std::random_device device;
        const uint64_t seed = static_cast<uint64_t>(device()) << 32 | static_cast<uint64_t>(device());
        std::mt19937_64 engine{seed};
        Distribution distribution = commonDistribution;

        Results results(iterations);

        {
            Timer timer{"Baseline", iterations, &baselineResult};
            for (auto& result : results) {
                result = distribution(engine);
            }
        }

        touchResults(results);
    }

    {
        Distribution distribution = commonDistribution;
        threaded_rng_cache::RngCache rngCache{distribution};

        Results results(iterations);

        {
            Timer timer{"RngCache", iterations, baselineResult};
            for (auto& result : results) {
                result = rngCache();
            }
        }

        touchResults(results);
    }


    return 0;
}
