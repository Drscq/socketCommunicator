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
