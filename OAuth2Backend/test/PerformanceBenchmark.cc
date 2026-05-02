#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <climits>
#include "../storage/MemoryOAuth2Storage.h"
#include "../plugins/OAuth2Plugin.h"

using namespace drogon;

// Performance measurement utilities
class PerformanceTimer
{
public:
    PerformanceTimer(const std::string &name)
        : name_(name), start_(std::chrono::high_resolution_clock::now()) {}

    ~PerformanceTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        LOG_INFO << "[PERF] " << name_ << " took " << duration.count() << " microseconds";
    }

    int64_t elapsed() const
    {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
    }

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

// Statistics calculation
struct PerformanceStats
{
    int64_t min_us = 999999999;
    int64_t max_us = 0;
    int64_t total_us = 0;
    int64_t count = 0;

    void addSample(int64_t microseconds)
    {
        min_us = (min_us < microseconds) ? min_us : microseconds;
        max_us = (max_us > microseconds) ? max_us : microseconds;
        total_us += microseconds;
        count++;
    }

    double avg_us() const { return count > 0 ? (double)total_us / count : 0.0; }

    double ops_per_second() const
    {
        double avg_sec = avg_us() / 1000000.0;
        return avg_sec > 0 ? 1.0 / avg_sec : 0.0;
    }

    std::string toString() const
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "Count: " << count << ", ";
        oss << "Min: " << min_us << "us, ";
        oss << "Max: " << max_us << "us, ";
        oss << "Avg: " << avg_us() << "us, ";
        oss << "Ops/s: " << ops_per_second();
        return oss.str();
    }
};

DROGON_TEST(Performance_OAuth2Flow)
{
    LOG_INFO << "=== Performance Test: OAuth2 Flow ===";

    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (!plugin)
    {
        LOG_WARN << "OAuth2Plugin not found. Skipping test.";
        return;
    }

    auto storage = std::make_shared<oauth2::MemoryOAuth2Storage>();
    Json::Value config;
    config["vue-client"]["secret"] = "test-secret";
    config["vue-client"]["redirect_uri"] = "http://localhost:5173/callback";
    config["vue-client"]["client_type"] = "public";
    storage->initFromConfig(config);

    const int ITERATIONS = 100;
    PerformanceStats saveStats, consumeStats, validateStats;

    try
    {
        // Benchmark: Save Auth Code
        LOG_INFO << "--- Benchmark: Save Auth Code (" << ITERATIONS << " iterations) ---";
        for (int i = 0; i < ITERATIONS; i++)
        {
            oauth2::OAuth2AuthCode code;
            code.code = "perf_test_" + std::to_string(i);
            code.clientId = "vue-client";
            code.userId = "test_user";
            code.expiresAt = std::time(nullptr) + 3600;
            code.used = false;
            code.redirectUri = "http://localhost:5173/callback";

            PerformanceTimer timer("Save Auth Code");
            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(code, [&]() { p.set_value(); });
            f.get();
            saveStats.addSample(timer.elapsed());
        }
        LOG_INFO << "Save Auth Code - " << saveStats.toString();

        // Benchmark: Consume Auth Code
        LOG_INFO << "--- Benchmark: Consume Auth Code (" << ITERATIONS << " iterations) ---";
        for (int i = 0; i < ITERATIONS; i++)
        {
            std::string codeStr = "perf_test_" + std::to_string(i);
            std::string redirectUri = "http://localhost:5173/callback";

            PerformanceTimer timer("Consume Auth Code");
            std::promise<std::optional<oauth2::OAuth2AuthCode>> p;
            auto f = p.get_future();
            storage->consumeAuthCode(
                codeStr,
                redirectUri,
                [&](std::optional<oauth2::OAuth2AuthCode> code) { p.set_value(code); });
            f.get();
            consumeStats.addSample(timer.elapsed());
        }
        LOG_INFO << "Consume Auth Code - " << consumeStats.toString();

        // Benchmark: Validate Client
        LOG_INFO << "--- Benchmark: Validate Client (" << ITERATIONS << " iterations) ---";
        for (int i = 0; i < ITERATIONS; i++)
        {
            PerformanceTimer timer("Validate Client");
            std::promise<bool> p;
            auto f = p.get_future();
            storage->validateClient(
                "vue-client",
                "test-secret",
                [&](bool valid) { p.set_value(valid); });
            f.get();
            validateStats.addSample(timer.elapsed());
        }
        LOG_INFO << "Validate Client - " << validateStats.toString();

        LOG_INFO << "=== Performance Test Completed ===";
        CHECK(saveStats.avg_us() < 1000);  // Save should be < 1ms
        CHECK(consumeStats.avg_us() < 1000);  // Consume should be < 1ms
        CHECK(validateStats.avg_us() < 500);  // Validate should be < 0.5ms
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Performance Test Failed: " << e.what();
        throw;
    }
}

DROGON_TEST(Performance_StorageThroughput)
{
    LOG_INFO << "=== Performance Test: Storage Throughput ===";

    auto storage = std::make_shared<oauth2::MemoryOAuth2Storage>();
    Json::Value config;
    config["vue-client"]["secret"] = "test-secret";
    config["vue-client"]["redirect_uri"] = "http://localhost:5173/callback";
    config["vue-client"]["client_type"] = "public";
    storage->initFromConfig(config);

    const int CONCURRENT_REQUESTS = 50;
    const int TOTAL_REQUESTS = 500;

    try
    {
        // Benchmark: Concurrent Save Operations
        LOG_INFO << "--- Benchmark: Concurrent Saves ("
                 << CONCURRENT_REQUESTS << " concurrent, "
                 << TOTAL_REQUESTS << " total) ---";

        std::vector<std::promise<void>> promises(TOTAL_REQUESTS);
        std::vector<std::future<void>> futures;
        for (auto &p : promises)
        {
            futures.push_back(p.get_future());
        }

        PerformanceTimer timer("Concurrent Saves");

        std::vector<std::thread> threads;
        int requestsPerThread = TOTAL_REQUESTS / CONCURRENT_REQUESTS;

        for (int t = 0; t < CONCURRENT_REQUESTS; t++)
        {
            threads.push_back(std::thread([&, t]() {
                for (int i = 0; i < requestsPerThread; i++)
                {
                    int idx = t * requestsPerThread + i;
                    if (idx >= TOTAL_REQUESTS) break;

                    oauth2::OAuth2AuthCode code;
                    code.code = "throughput_test_" + std::to_string(idx);
                    code.clientId = "vue-client";
                    code.userId = "test_user";
                    code.expiresAt = std::time(nullptr) + 3600;
                    code.used = false;
                    code.redirectUri = "http://localhost:5173/callback";

                    storage->saveAuthCode(code, [&]() {
                        promises[idx].set_value();
                    });
                }
            }));
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        for (auto &future : futures)
        {
            future.get();
        }

        int64_t elapsed_us = timer.elapsed();
        double throughput = (double)TOTAL_REQUESTS / (elapsed_us / 1000000.0);

        LOG_INFO << "Completed " << TOTAL_REQUESTS << " saves in "
                 << elapsed_us << "us";
        LOG_INFO << "Throughput: " << throughput << " ops/sec";

        CHECK(throughput > 1000);  // Should handle > 1000 ops/sec
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Performance Test Failed: " << e.what();
        throw;
    }
}

DROGON_TEST(Performance_MemoryUsage)
{
    LOG_INFO << "=== Performance Test: Memory Usage ===";

    auto storage = std::make_shared<oauth2::MemoryOAuth2Storage>();
    Json::Value config;
    config["vue-client"]["secret"] = "test-secret";
    config["vue-client"]["redirect_uri"] = "http://localhost:5173/callback";
    config["vue-client"]["client_type"] = "public";
    storage->initFromConfig(config);

    const int NUM_CODES = 10000;

    try
    {
        LOG_INFO << "--- Memory Usage Test (" << NUM_CODES << " auth codes) ---";

        // Save many auth codes
        for (int i = 0; i < NUM_CODES; i++)
        {
            oauth2::OAuth2AuthCode code;
            code.code = "mem_test_" + std::to_string(i);
            code.clientId = "vue-client";
            code.userId = "test_user";
            code.expiresAt = std::time(nullptr) + 3600;
            code.used = false;
            code.redirectUri = "http://localhost:5173/callback";

            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(code, [&]() { p.set_value(); });
            f.get();
        }

        LOG_INFO << "Successfully stored " << NUM_CODES << " auth codes";
        LOG_INFO << "Approximate memory usage: "
                 << (NUM_CODES * sizeof(oauth2::OAuth2AuthCode)) / 1024 << " KB";

        // Verify we can retrieve them
        int found = 0;
        for (int i = 0; i < 100; i++)
        {
            int idx = i * (NUM_CODES / 100);
            std::string codeStr = "mem_test_" + std::to_string(idx);

            std::promise<std::optional<oauth2::OAuth2AuthCode>> p;
            auto f = p.get_future();
            storage->getAuthCode(
                codeStr,
                [&](std::optional<oauth2::OAuth2AuthCode> code) {
                    if (code.has_value()) found++;
                    p.set_value(code);
                });
            f.get();
        }

        LOG_INFO << "Verified " << found << "/100 sampled codes";
        CHECK(found == 100);

    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Performance Test Failed: " << e.what();
        throw;
    }
}

DROGON_TEST(Performance_LatencyPercentiles)
{
    LOG_INFO << "=== Performance Test: Latency Percentiles ===";

    auto storage = std::make_shared<oauth2::MemoryOAuth2Storage>();
    Json::Value config;
    config["vue-client"]["secret"] = "test-secret";
    config["vue-client"]["redirect_uri"] = "http://localhost:5173/callback";
    config["vue-client"]["client_type"] = "public";
    storage->initFromConfig(config);

    const int SAMPLES = 1000;
    std::vector<int64_t> latencies;

    try
    {
        LOG_INFO << "--- Latency Percentiles Test (" << SAMPLES << " samples) ---";

        for (int i = 0; i < SAMPLES; i++)
        {
            oauth2::OAuth2AuthCode code;
            code.code = "pct_test_" + std::to_string(i);
            code.clientId = "vue-client";
            code.userId = "test_user";
            code.expiresAt = std::time(nullptr) + 3600;
            code.used = false;
            code.redirectUri = "http://localhost:5173/callback";

            auto start = std::chrono::high_resolution_clock::now();

            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(code, [&]() { p.set_value(); });
            f.get();

            auto end = std::chrono::high_resolution_clock::now();
            int64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            latencies.push_back(latency);
        }

        std::sort(latencies.begin(), latencies.end());

        auto percentile = [&](int p) -> int64_t {
            int idx = (SAMPLES * p) / 100;
            return latencies[idx];
        };

        LOG_INFO << "Latency Percentiles (microseconds):";
        LOG_INFO << "  P50 (Median): " << percentile(50);
        LOG_INFO << "  P90: " << percentile(90);
        LOG_INFO << "  P95: " << percentile(95);
        LOG_INFO << "  P99: " << percentile(99);
        LOG_INFO << "  P99.9: " << percentile(999) / 10.0;

        CHECK(percentile(99) < 1000);  // P99 should be < 1ms

    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Performance Test Failed: " << e.what();
        throw;
    }
}
