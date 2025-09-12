#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

// NetIOMP local headers
#include "netmp.h"

// Timing test similar to CommunicatorTest.TimingOfDealerSendAcrossPayloadSizes
// Uses 2 parties (1 sender, 2 receiver/ACK).

TEST(NetIOMPTest, TimingAcrossPayloadSizes) {
    using clock = std::chrono::steady_clock;

    // Base port chosen to avoid collisions with other tests
    const int base_port = 42000;

    // Payload sizes (bytes)
    const std::vector<size_t> sizes = { 8u, 64u, 512u, 4096u, 32768u, 262144u, 1048576u };

    const int iterations_warmup = 5;
    const int iterations = 1000; // keep runtime snappy in CI

    // Party 2: receiver + ACK back to party 1
    std::thread t2([&]() {
        NetIOMP<2> io(2, base_port);
        for (size_t sz : sizes) {
            std::vector<char> buf(sz);
            // warmup
            for (int i = 0; i < iterations_warmup; ++i) {
                io.recv_data(1, buf.data(), buf.size());
                char ack = 'a';
                io.send_data(1, &ack, 1);
                io.flush();
            }
            // timed
            for (int i = 0; i < iterations; ++i) {
                io.recv_data(1, buf.data(), buf.size());
                char ack = 'a';
                io.send_data(1, &ack, 1);
                io.flush();
            }
        }
    });

    // Create party 1 in main thread right after launching party 2
    NetIOMP<2> io1(1, base_port);

    // Give sockets a brief moment to finish handshakes
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Network characteristics for theoretical timing (ms)
    const double bandwidth_gbps = 14.4;   // 14.4 Gbps
    const double rtt_avg_ms = 0.038;      // average RTT in ms

    for (size_t sz : sizes) {
        std::vector<char> payload(sz, 'x');
        char ack = 0;

        // Warmup
        for (int i = 0; i < iterations_warmup; ++i) {
            io1.send_data(2, payload.data(), payload.size());
            io1.recv_data(2, &ack, 1);
        }

        // Timed round-trips
        auto start_t = clock::now();
        for (int i = 0; i < iterations; ++i) {
            io1.send_data(2, payload.data(), payload.size());
            io1.recv_data(2, &ack, 1);
            ASSERT_EQ(ack, 'a');
        }
        auto end_t = clock::now();

        const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
        const double avg_ms = (static_cast<double>(duration_ns) / iterations) / 1e6; // round-trip
        const double theoretical_ms = (static_cast<double>(sz) * 8.0) / (bandwidth_gbps * 1e6) + rtt_avg_ms;
        const double diff_ms = avg_ms - theoretical_ms;
        const double pct = theoretical_ms > 0.0 ? (diff_ms / theoretical_ms) * 100.0 : 0.0;

        std::cout << "[NetIOMP] Size " << sz
                  << " bytes: avg end-to-end (send+ACK) = " << avg_ms
                  << " ms, theoretical (RTT-based) = " << theoretical_ms
                  << " ms, diff = " << diff_ms << " ms (" << pct << "%)"
                  << std::endl;
    }

    if (t2.joinable()) t2.join();
}

// Helper to run the party-count timing test for a compile-time party count N
template <int N>
static void run_partycount_timing(int base_port, size_t payload_size, int iterations_warmup, int iterations) {
    using clock = std::chrono::steady_clock;

    const int sender_id = 1;
    const int num_receivers = N - 1;
    const int port = base_port; // caller can vary if desired

    // Launch receiver threads 2..N
    std::vector<std::thread> rxs;
    for (int rid = 2; rid <= N; ++rid) {
        rxs.emplace_back([=]() {
            NetIOMP<N> io(rid, port);
            std::vector<char> buf(payload_size);
            const int total_rounds = iterations_warmup + iterations;
            for (int i = 0; i < total_rounds; ++i) {
                io.recv_data(sender_id, buf.data(), buf.size());
                char ack = 'a';
                io.send_data(sender_id, &ack, 1);
                io.flush();
            }
        });
    }

    // Create sender in main thread after launching receivers
    NetIOMP<N> S(sender_id, port);

    // Brief settle time for connections
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<char> payload(payload_size, 'x');

    // Warmup rounds: send to all then wait for all ACKs
    for (int w = 0; w < iterations_warmup; ++w) {
        for (int rid = 2; rid <= N; ++rid) {
            S.send_data(rid, payload.data(), payload.size());
        }
        S.flush();
        for (int rid = 2; rid <= N; ++rid) {
            char ack = 0;
            S.recv_data(rid, &ack, 1);
        }
    }

    // Timed rounds: send to all then wait for all ACKs
    auto start = clock::now();
    for (int it = 0; it < iterations; ++it) {
        for (int rid = 2; rid <= N; ++rid) {
            S.send_data(rid, payload.data(), payload.size());
        }
        S.flush();
        for (int k = 0; k < num_receivers; ++k) {
            // ACK order may vary; read from each receiver deterministically
            int rid = 2 + k;
            char ack = 0;
            S.recv_data(rid, &ack, 1);
        }
    }
    auto end = clock::now();

    const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const double avg_ms_per_round = (static_cast<double>(duration_ns) / iterations) / 1e6;
    std::cout << "[NetIOMP] Parties " << N
              << ": avg round (send-all+ACKs) = " << avg_ms_per_round
              << " ms (payload 1MB)" << std::endl;

    for (auto& t : rxs) if (t.joinable()) t.join();
}

TEST(NetIOMPTest, TimingAcrossPartyCounts) {
    // Mirror CommunicatorTest.TimingOfDealerSendAcrossPartyCounts
    const std::vector<int> party_counts = {2, 4, 6, 8, 10};
    const size_t payload_size = 1024 * 1024; // 1MB
    const int iterations_warmup = 5;
    const int iterations = 200; // match Communicator test balance

    // Use distinct base port to avoid any potential overlap with other tests
    const int base_port = 42100;

    for (int np : party_counts) {
        // For each party count, dispatch to the appropriate template instantiation
        const int port = base_port + np * 10; // spread ports a bit between runs
        switch (np) {
            case 2:  run_partycount_timing<2>(port,  payload_size, iterations_warmup, iterations);  break;
            case 4:  run_partycount_timing<4>(port,  payload_size, iterations_warmup, iterations);  break;
            case 6:  run_partycount_timing<6>(port,  payload_size, iterations_warmup, iterations);  break;
            case 8:  run_partycount_timing<8>(port,  payload_size, iterations_warmup, iterations);  break;
            case 10: run_partycount_timing<10>(port, payload_size, iterations_warmup, iterations);  break;
            default: FAIL() << "Unsupported party count in test: " << np; break;
        }
    }
}
