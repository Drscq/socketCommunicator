#include <gtest/gtest.h>
#include "Communicator.h"
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <numeric>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <random> 



// Threaded variant: run each party in its own thread to avoid shared-socket identity races
TEST(MPCPartiesTest, NPartyAllToAllSumThreaded) {
    const int N = 20;
    const int base = 15000; // separate port base
    const std::string host = "127.0.0.1";

    // Each party i holds value i+1
    const int expected_sum = (N * (N + 1)) / 2; // 1..N
    std::vector<int> totals(N, 0);
    std::vector<std::thread> threads;
    threads.reserve(N);
    std::vector<bool> okFlags(N, true);

    for (int i = 0; i < N; ++i) {
    threads.emplace_back([i, N, base, host, &totals, &okFlags]() {
            const int id = i + 1;
            Communicator me(id, base, host, N);
            me.setUpRouterDealer();

            // Send my value to everyone else using dedicated per-peer DEALERs only
            const std::string myVal = std::to_string(id);
            // for (int peer = 1; peer <= N; ++peer) {
            //     if (peer == id) continue;
            //     if (!me.dealerSendTo(peer, myVal)) {
            //         okFlags[i] = false;
            //     }
            // }
            if (!me.dealerSendToAllParallel(myVal)) {
                okFlags[i] = false;
            }

            // Receive N-1 values and compute local total
            int sum_peer = 0;
            for (int r = 0; r < N - 1; ++r) {
                std::string from, payload;
                if (!me.routerReceive(from, payload, -1)) {
                    okFlags[i] = false;
                    break;
                }
                sum_peer += std::stoi(payload);
            }
            totals[i] = sum_peer + id;
        });
    }

    for (auto& t : threads) t.join();
    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(okFlags[i]) << "party " << (i+1) << " had a comms failure";
        EXPECT_EQ(totals[i], expected_sum) << "party " << (i+1) << " wrong total";
    }
}

// Helper function to run a communication test and measure send time
static std::chrono::milliseconds run_send_test(int N, const std::string& data, bool use_sequential_send) {
    const int base_port = 17000;
    const std::string host = "127.0.0.1";
    std::vector<std::thread> threads;
    threads.reserve(N);
    std::vector<bool> okFlags(N, true);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([i, N, base_port, host, &data, use_sequential_send, &okFlags]() {
            const int id = i + 1;
            Communicator me(id, base_port, host, N);
            me.setUpRouterDealer();

            if (use_sequential_send) {
                for (int peer = 1; peer <= N; ++peer) {
                    if (peer == id) continue;
                    if (!me.dealerSendTo(peer, data)) {
                        okFlags[i] = false;
                    }
                }
            } else {
                if (!me.dealerSendToAllParallel(data)) {
                    okFlags[i] = false;
                }
            }

            // Still need to receive messages to allow sends to complete
            for (int r = 0; r < N - 1; ++r) {
                std::string from, payload;
                if (!me.routerReceive(from, payload, -1)) {
                    okFlags[i] = false;
                    break;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
}

TEST(MPCPartiesTest, SendToAllPerformanceComparison) {
    std::vector<int> party_counts = {2, 4, 6, 8, 10}; // number of parties
    // std::vector<size_t> data_sizes = {4096, 16384, 65536, 262144, 1048576}; // 4KB, 16KB, 64KB, 256KB, 1MB
    std::vector<size_t> data_sizes = {1048576};
    const int iterations = 10; // repeat runs to stabilize measurements

    std::cout << std::endl;
    std::cout << "--- Send-To-All Performance Comparison ---" << std::endl;
    std::cout << "N, DataSize (B), Sequential (ms), Parallel (ms)" << std::endl;

    for (int n : party_counts) {
        for (size_t size : data_sizes) {
            std::string data(size, 'x');

            long long seq_total_ms = 0;
            long long par_total_ms = 0;

            for (int it = 0; it < iterations; ++it) {
                auto seq_duration = run_send_test(n, data, true);
                // small pause between runs to avoid port/socket reuse flakiness
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                auto par_duration = run_send_test(n, data, false);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                seq_total_ms += seq_duration.count();
                par_total_ms += par_duration.count();
            }

            double seq_avg = static_cast<double>(seq_total_ms) / iterations;
            double par_avg = static_cast<double>(par_total_ms) / iterations;

            std::cout << std::fixed << std::setprecision(1)
                      << n << ", " << size << ", " << seq_avg << ", " << par_avg << std::endl;
        }
    }
    std::cout << "----------------------------------------" << std::endl;
}


TEST(MPCPartiesTest, addition_delay) {
    int64_t a;
    int64_t b;
    int64_t Q = 8380417; // a prime number
    // get a random 64-bit integer
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int64_t> dis;
    int64_t c;
    std::chrono::_V2::system_clock::time_point start;
    std::chrono::_V2::system_clock::time_point end;
    std::chrono::_V2::system_clock::rep total_duration_add = 0, total_duration_mul = 0;
    for (int i = 0; i < 100; i++) {
         a = dis(gen);
         b = dis(gen);
        start = std::chrono::high_resolution_clock::now();
        c = (a + b) % Q;
        end = std::chrono::high_resolution_clock::now();
        total_duration_add += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
    total_duration_add = total_duration_add / 100;
    std::cout << "Addition took " << total_duration_add << " nanoseconds." << std::endl;
    EXPECT_EQ(c, (a + b) % Q);

    int64_t d;
    for (int i = 0; i < 100; i++) {
        a = dis(gen);
        b = dis(gen);

        start = std::chrono::high_resolution_clock::now();
        d = (a * b) % Q;
        end = std::chrono::high_resolution_clock::now();
        total_duration_mul += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
    total_duration_mul = total_duration_mul / 100;
    std::cout << "Multiplication took " << total_duration_mul << " nanoseconds." << std::endl;
    EXPECT_EQ(d, (a * b) % Q);
}

