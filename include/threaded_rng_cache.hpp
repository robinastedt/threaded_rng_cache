/*
MIT License

Copyright (c) 2023 Robin Åstedt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <ranges>
#include <optional>
#include <functional>
#include <stdexcept>
#include <cassert>

namespace threaded_rng_cache
{
    template<typename DistributionT,
             typename EngineT = std::mt19937_64,
             size_t CHUNK_SIZE = /* 128 KiB */ 128 * 1024 / sizeof(typename DistributionT::result_type)>
    class RngCache {
    public:
        using result_type = DistributionT::result_type;
        using seed_type = EngineT::result_type;

        RngCache(
            const DistributionT& distribution,
            std::optional<seed_type> seed = {},
            std::optional<size_t> threadCount = {})
        : RngCache(
            distribution,
            seed ? *seed : randomSeed(),
            threadCount ? *threadCount : std::thread::hardware_concurrency())
        {}

        result_type
        operator()() {
            return generate();
        }

    private:
        RngCache(
            const DistributionT& distribution,
            seed_type seed,
            size_t threadCount)
        : m_activeChunk(std::make_unique<Chunk>())
        , m_producers(Producer::create(distribution, seed, threadCount))
        , m_nextProducer(m_producers.begin())
        {}

        class Chunk {
        public:
            using pointer = std::unique_ptr<Chunk>;

            Chunk()
            : m_nextIndex(CHUNK_SIZE)
            , m_values()
            {}

            result_type
            next() {
                assert(!empty());
                return m_values[m_nextIndex++];
            }

            bool
            empty() const {
                return m_nextIndex == m_values.size();
            }

            bool
            full() const {
                return m_nextIndex == 0u;
            }

            void
            fill(std::function<result_type(void)> generator) {
                std::ranges::generate(m_values, generator);
                m_nextIndex = 0;
            }

        private:
            using storage_t = std::array<result_type, CHUNK_SIZE>;

            size_t m_nextIndex;
            storage_t m_values;
        };

        class Producer {
        public:
            using pointer = std::unique_ptr<Producer>;
            using container = std::vector<pointer>;

            Producer(const DistributionT& distribution, seed_type seed)
            : m_mutex()
            , m_conditionVariable()
            , m_shutdown(false)
            , m_chunk(std::make_unique<Chunk>())
            , m_distribution(distribution)
            , m_engine(seed)
            , m_thread([this](){ run(); })
            {}

            ~Producer() {
                stop();
            }

            void
            swapChunk(Chunk::pointer& otherChunk) {
                {
                    std::unique_lock lock{m_mutex};
                    m_conditionVariable.wait(lock, [this](){ return swapWaitCondition(); });
                    if (m_shutdown) {
                        throw std::logic_error{"threaded_rng_cache::RngCache: Illegal access of closed instance."};
                    }
                    std::swap(m_chunk, otherChunk);
                }
                m_conditionVariable.notify_one();
            }

            static container
            create(const DistributionT& distribution, seed_type seed, size_t count) {
                EngineT rootEngine{seed};
                container producers{count};
                std::ranges::generate(producers, [&](){
                    const seed_type childSeed = rootEngine();
                    return std::make_unique<Producer>(distribution, childSeed);
                });
                return producers;
            }
        private:

            void
            stop() {
                {
                    std::lock_guard lock{m_mutex};
                    m_shutdown = true;
                }
                m_conditionVariable.notify_one();
                m_thread.join();
            }

            bool
            swapWaitCondition() const {
                return m_shutdown || m_chunk->full();
            }

            bool
            fillWaitCondition() const {
                return m_shutdown || m_chunk->empty();
            }

            void
            run() {
                while (true) {
                    {
                        std::unique_lock lock{m_mutex};
                        m_conditionVariable.wait(lock, [this](){ return fillWaitCondition(); });
                        if (m_shutdown) {
                            return;
                        }
                        m_chunk->fill([this](){ return generate(); });
                    }
                    m_conditionVariable.notify_one();
                }
            }

            result_type
            generate() {
                return m_distribution(m_engine);
            }

            std::mutex m_mutex;
            std::condition_variable m_conditionVariable;
            bool m_shutdown;
            Chunk::pointer m_chunk;
            DistributionT m_distribution;
            EngineT m_engine;
            std::thread m_thread;
        };

        static seed_type
        randomSeed() {
            using Device = std::random_device;

            static_assert(std::is_unsigned_v<seed_type>);
            static_assert(std::is_unsigned_v<Device::result_type>);

            constexpr size_t seedSize = sizeof(seed_type);
            constexpr size_t deviceSize = sizeof(Device::result_type);
            constexpr size_t iterations = (seedSize - 1) / deviceSize;

            Device device;
            seed_type seed = device();

            for (size_t i = 0; i < iterations; ++i) {
                seed = (seed << deviceSize) | device();
            }
            return seed;
        }

        Producer&
        nextProducer() {
            Producer& producer = **m_nextProducer++;
            if (m_nextProducer == m_producers.end()) {
                m_nextProducer = m_producers.begin();
            }
            return producer;
        }

        result_type
        generate() {
            if (m_activeChunk->empty()) {
                Producer& producer = nextProducer();
                producer.swapChunk(m_activeChunk);
            }
            return m_activeChunk->next();
        }

        Chunk::pointer m_activeChunk;
        Producer::container m_producers;
        Producer::container::iterator m_nextProducer;
    };


} // namespace threaded_rng_cache
