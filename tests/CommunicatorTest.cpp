#include <gtest/gtest.h>
#include "Communicator.h"
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#define BASE_PORT 10000
TEST(CommunicatorTest, ConstructorStoresValues) {
    Communicator c{42, 5000, "192.168.1.10"};
    EXPECT_EQ(c.getId(), 42);
    EXPECT_EQ(c.getPortBase(), 5000);
    EXPECT_EQ(c.getAddress(), std::string("192.168.1.10"));
}

TEST(CommunicatorTest, SetUpRouterDoesNotThrow) {
    // Test that setUpRouter can be called without throwing exceptions
    Communicator c{1, 9000, "127.0.0.1"};
    EXPECT_NO_THROW(c.setUpRouter());
}

TEST(CommunicatorTest, DealerSendToTargetsSpecificPeer) {
    const int base = 9900;
    const int num_parties = 3;
    Communicator A{1, base, "127.0.0.1", num_parties};
    Communicator B{2, base, "127.0.0.1", num_parties};
    Communicator C{3, base, "127.0.0.1", num_parties};

    A.setUpRouter();
    B.setUpRouter();
    C.setUpRouter();
    A.setUpPerPeerDealers();
    B.setUpPerPeerDealers();
    C.setUpPerPeerDealers();

    // A will create dedicated sockets per peer as needed
    // No need to call setUpDealer on A for this test

    // Send two messages: one to B, one to C
    ASSERT_TRUE(A.dealerSendTo(2, "to-B"));
    ASSERT_TRUE(A.dealerSendTo(3, "to-C"));

    std::string from, msg;
    ASSERT_TRUE(B.routerReceive(from, msg, 1000));
    EXPECT_EQ(from, std::to_string(1));
    EXPECT_EQ(msg, "to-B");

    ASSERT_TRUE(C.routerReceive(from, msg, 1000));
    EXPECT_EQ(from, std::to_string(1));
    EXPECT_EQ(msg, "to-C");
}

TEST(CommunicatorTest, TimingOfDealerSendToTargetsSpecificPeer) {
    const int num_parties = 2;
    // Create the sender Communicator in this (main) thread, but delay dealer setup
    // until the receiver thread has fully initialized its Router to respect ZMQ thread affinity.
    Communicator A{1, BASE_PORT, "127.0.0.1", num_parties};

    // Fixed 1MB payload for all iterations
    std::string payload(1024 * 1024, 'x'); // 1MB message
    // Run the receiver on B in a dedicated thread while A sends in this thread.
    using clock = std::chrono::steady_clock;

    const int iterations = 1000;
    const int iterations_warmup = 5;
    const int total_expected = iterations_warmup + iterations;

    std::thread rx([&](){
        // All ZMQ sockets for B are created and used within this thread.
        Communicator B{2, BASE_PORT, "127.0.0.1", num_parties};
        B.setUpRouterDealer();
        std::string from; (void)from;
        std::string msg;
        for (int i = 0; i < total_expected; ++i) {
            // Block until a message arrives (set a long-ish timeout to avoid flakes in CI)
            bool ok = B.routerReceive(from, msg, -1);
            if (!ok) break;
        }
    });

    // After receiver is ready, set up A's dealers so connects happen after B is bound
    A.setUpRouterDealer();

    // Warmup sends (not timed)
    for (int i = 0; i < iterations_warmup; ++i) {
        zmq::message_t warm_msg(payload.data(), payload.size());
        ASSERT_TRUE(A.dealerSendTo(2, std::move(warm_msg)));
    }

    // Timed burst of N sends while receiver drains concurrently
    auto start = clock::now();
    for (int i = 0; i < iterations; ++i) {
        zmq::message_t send_msg(payload.data(), payload.size());
        ASSERT_TRUE(A.dealerSendTo(2, std::move(send_msg)));
    }

    if (rx.joinable()) rx.join();
    auto end = clock::now();

    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double avg_ns = static_cast<double>(duration_ns) / iterations;
    std::cout << "Measured dealerSendTo avg enqueue time: " << avg_ns / 1e6 << " ms" << std::endl;
    // Theoretical delay calculation
    // bandwidth 14.4 Gbps
    double bandwidth_gbps = 14.4;
    // RTT min/avg/max/mdev = 0.022/0.038/0.057/0.010 ms
    double rtt_avg = 0.038;
    std::cout << "Message Size: " << payload.size() << " bytes" << std::endl;
    double theoretical_time_ms = (payload.size() * 8) / (bandwidth_gbps * 1e6) + rtt_avg / 2;
    std::cout << "Theoretical dealerSendTo time: " << theoretical_time_ms << " ms" << std::endl;
}

TEST(CommunicatorTest, TimingOfDealerSendAcrossPayloadSizes) {
    const int num_parties = 2;
    Communicator A{1, BASE_PORT, "127.0.0.1", num_parties};

    using clock = std::chrono::steady_clock;
    const int iterations_warmup = 5;
    // Single knob to control iterations across all payload sizes
    const int iterations = 1000; // adjust as needed for runtime/precision trade-off

    // Sweep sizes from 8 bytes up to 1MB (log-spaced, coarse to keep runtime reasonable)
    const std::vector<size_t> sizes = {
        8u, 64u, 512u, 4096u, 32768u, 262144u, 1048576u
    };

    // Receiver thread: drain all messages across all sizes
    const size_t total_expected = sizes.size() * static_cast<size_t>(iterations_warmup + iterations);

    std::thread rx([&](){
        Communicator B{2, BASE_PORT, "127.0.0.1", num_parties};
        B.setUpRouterDealer();
        std::string from; (void)from;
        std::string msg;
        for (size_t i = 0; i < total_expected; ++i) {
            // Receive from A's DEALER on B's ROUTER
            if (!B.routerReceive(from, msg, -1)) break;
            // Echo back a small ACK from B's DEALER to A's ROUTER to complete round-trip
            int toId = 1;
            (void)B.dealerSendTo(toId, "a");
        }
    });

    A.setUpRouterDealer();

    // Network characteristics for theoretical timing (ms)
    const double bandwidth_gbps = 14.4;   // 14.4 Gbps
    const double rtt_avg_ms = 0.038;      // average RTT in ms

    for (size_t idx = 0; idx < sizes.size(); ++idx) {
        const size_t sz = sizes[idx];

        std::string payload(sz, 'x');

        // Warmup: send then wait for ACK to avoid queue buildup (not timed)
        for (int i = 0; i < iterations_warmup; ++i) {
            zmq::message_t warm_msg(payload.data(), payload.size());
            ASSERT_TRUE(A.dealerSendTo(2, std::move(warm_msg)));
            std::string fromAck, ack;
            ASSERT_TRUE(A.routerReceive(fromAck, ack, -1));
        }

    // Timed end-to-end (send + ACK receive) round-trips
    std::string fromAck, ack;
        auto start = clock::now();
        for (int i = 0; i < iterations; ++i) {
            zmq::message_t send_msg(payload.data(), payload.size());
            A.dealerSendTo(2, std::move(send_msg));
            A.routerReceive(fromAck, ack, -1);
        }
        auto end = clock::now();

        const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        const double avg_ms = (static_cast<double>(duration_ns) / iterations) / 1e6; // ns -> ms (round-trip)

        // Theoretical round-trip in ms: serialization (outbound) + RTT (assume tiny ACK)
        const double theoretical_ms = (static_cast<double>(sz) * 8.0) / (bandwidth_gbps * 1e6) + rtt_avg_ms;
        const double diff_ms = avg_ms - theoretical_ms;
        const double pct = theoretical_ms > 0.0 ? (diff_ms / theoretical_ms) * 100.0 : 0.0;

    std::cout << "Size " << sz << " bytes: avg end-to-end (send+ACK) = " << avg_ms
                  << " ms, theoretical (RTT-based) = " << theoretical_ms
                  << " ms, diff = " << diff_ms << " ms (" << pct << "%)" << " with iteractions " << iterations << std::endl;
    }

    if (rx.joinable()) rx.join();
}

TEST(CommunicatorTest, TimingOfDealerSendAcrossPartyCounts) {
    // Measure timing as number of parties increases; party 1 sends 1MB to all others
    using clock = std::chrono::steady_clock;
    const std::vector<int> party_counts = {2, 4, 6, 8, 10};
    const size_t payload_size = 1024 * 1024; // 1MB
    const int iterations_warmup = 5;
    const int iterations = 200; // keep runtime reasonable while averaging

    for (int num_parties : party_counts) {
        const int senderId = 1;
        const int num_receivers = num_parties - 1;

        // Prepare payload
        std::string payload(payload_size, 'x');
         // Sender in main thread
        Communicator S{senderId, BASE_PORT, "127.0.0.1", num_parties};
        S.setUpRouterDealer();

    // Launch receiver threads (2..num_parties). Each has its own sockets in its own thread.
        std::vector<std::thread> rxs;

        for (int rid = 2; rid <= num_parties; ++rid) {
            rxs.emplace_back([&, rid]() {
                Communicator R{rid, BASE_PORT, "127.0.0.1", num_parties};
                R.setUpRouterDealer();
                std::string from; std::string msg;
                // Each receiver processes all warmup + timed messages, ACKing each one
                const int total_rounds = iterations_warmup + iterations;
                for (int i = 0; i < total_rounds; ++i) {
                    // Receive from sender; long timeout to avoid flakes
                    if (!R.routerReceive(from, msg, -1)) break;
                    // ACK back to sender
                    (void)R.dealerSendTo(senderId, "a");
                }
            });
        }


        // Warmup: for stability, send to each receiver then wait for that many ACKs
        for (int w = 0; w < iterations_warmup; ++w) {
            for (int rid = 2; rid <= num_parties; ++rid) {
                zmq::message_t warm_msg(payload.data(), payload.size());
                ASSERT_TRUE(S.dealerSendTo(rid, std::move(warm_msg)));
            }
            for (int rid = 2; rid <= num_parties; ++rid) {
                std::string fromAck, ack;
                ASSERT_TRUE(S.routerReceive(fromAck, ack, -1));
            }
        }

        // Timed: Send to all receivers and wait for num_receivers ACKs, repeat iterations times
        std::string fromAck, ack;
        auto start = clock::now();
        for (int it = 0; it < iterations; ++it) {
            for (int rid = 2; rid <= num_parties; ++rid) {
                zmq::message_t msg(payload.data(), payload.size());
                S.dealerSendTo(rid, std::move(msg));
            }
            for (int k = 0; k < num_receivers; ++k) {
                S.routerReceive(fromAck, ack, -1);
            }
        }
        auto end = clock::now();

        const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        const double avg_ms_per_round = (static_cast<double>(duration_ns) / iterations) / 1e6; // one round = send to all + wait all ACKs
        std::cout << "Parties " << num_parties
                  << ": avg round (send-all+ACKs) = " << avg_ms_per_round << " ms (payload 1MB)" << std::endl;

        for (auto& t : rxs) if (t.joinable()) t.join();
    }
}

TEST(CommunicatorTest, DealerSendToAllParallelSendsToAllPeers) {
    const int num_parties = 5; // 1 sender + 4 receivers
    const int senderId = 1;
    const int size = 1024 * 1024;
    std::string payload(size, 'x'); // 1MB message

    // Synchronize readiness of receiver ROUTER sockets before sender connects
    const int num_receivers = num_parties - 1;

    // Storage for what each receiver observed
    std::vector<std::string> fromById(num_parties + 1);
    std::vector<std::string> msgById(num_parties + 1);

    std::vector<std::thread> rxs;
    for (int rid = 2; rid <= num_parties; ++rid) {
        rxs.emplace_back([&, rid]() {
            // Create each receiver's sockets within its own thread (ZMQ thread affinity)
            Communicator R{rid, BASE_PORT, "127.0.0.1", num_parties};
            R.setUpRouter();
            std::string from; std::string msg;
            // Wait for the sender's message
            bool ok = R.routerReceive(from, msg);
            if (ok) {
                fromById[rid] = from;
                msgById[rid] = msg;
            }
        });
    }


    // Sender in the main thread: prepare per-peer DEALER sockets and send in parallel
    Communicator S{senderId, BASE_PORT, "127.0.0.1", num_parties};
    S.setUpPerPeerDealers();

    // Give ZeroMQ a brief moment to finish handshakes to avoid dontwait send races
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(S.dealerSendToAllParallel(payload));

    for (auto& t : rxs) if (t.joinable()) t.join();
    auto end = std::chrono::steady_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double avg_ns = static_cast<double>(duration_ns);
    std::cout << "Measured dealerSendToAllParallel total enqueue time: " << avg_ns / 1e6 << " ms" << std::endl;

    // Verify every receiver got the payload from the sender's identity
    for (int rid = 2; rid <= num_parties; ++rid) {
        EXPECT_EQ(fromById[rid], std::to_string(senderId)) << "receiver id=" << rid;
        EXPECT_EQ(msgById[rid], payload) << "receiver id=" << rid;
    }
}

TEST(CommunicatorTest, TimingOfDealerSendToAllParallelAcrossPartyCounts) {
    // Measure timing as number of parties increases using dealerSendToAllParallel; sender 1 sends 1MB to all others
    using clock = std::chrono::steady_clock;
    const std::vector<int> party_counts = {2, 4, 6, 8, 10};
    const size_t payload_size = 1024 * 1024; // 1MB
    const int iterations_warmup = 5;
    const int iterations = 200; // balance runtime vs precision

    for (int num_parties : party_counts) {
        const int senderId = 1;
        const int num_receivers = num_parties - 1;

        std::string payload(payload_size, 'x');

        // Sender in main thread
        Communicator S{senderId, BASE_PORT, "127.0.0.1", num_parties};
        S.setUpRouterDealer();

        // Receivers: each loops through all warmup+timed rounds and ACKs each message
        std::vector<std::thread> rxs;
        for (int rid = 2; rid <= num_parties; ++rid) {
            rxs.emplace_back([&, rid]() {
                Communicator R{rid, BASE_PORT, "127.0.0.1", num_parties};
                R.setUpRouterDealer();
                std::string from; std::string msg;
                const int total_rounds = iterations_warmup + iterations;
                for (int i = 0; i < total_rounds; ++i) {
                    if (!R.routerReceive(from, msg, -1)) break;
                    (void)R.dealerSendTo(senderId, "a");
                }
            });
        }

        // Let connections settle briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Warmup rounds
        for (int w = 0; w < iterations_warmup; ++w) {
            ASSERT_TRUE(S.dealerSendToAllParallel(payload));
            for (int k = 0; k < num_receivers; ++k) {
                std::string fromAck, ack;
                ASSERT_TRUE(S.routerReceive(fromAck, ack, -1));
            }
        }

        // Timed rounds
        std::string fromAck, ack;
        auto start = clock::now();
        for (int it = 0; it < iterations; ++it) {
            ASSERT_TRUE(S.dealerSendToAllParallel(payload));
            for (int k = 0; k < num_receivers; ++k) {
                S.routerReceive(fromAck, ack, -1);
            }
        }
        auto end = clock::now();

        const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        const double avg_ms_per_round = (static_cast<double>(duration_ns) / iterations) / 1e6;
        std::cout << "[Parallel] Parties " << num_parties
                  << ": avg round (send-all+ACKs) = " << avg_ms_per_round << " ms (payload 1MB)" << std::endl;

        for (auto& t : rxs) if (t.joinable()) t.join();
    }
}

TEST(CommunicatorTest, PubSubBroadcastDeliversToAll) {
    const int num_parties = 5;
    const int senderId = 1;
    const std::string payload(256 * 1024, 'b'); // 256KB

    // Start receivers with SUB sockets
    std::vector<std::thread> rxs;
    std::vector<std::string> got(num_parties + 1);
    for (int rid = 2; rid <= num_parties; ++rid) {
        rxs.emplace_back([&, rid]() {
            Communicator R{rid, BASE_PORT, "127.0.0.1", num_parties};
            R.setUpSubscribers();
            std::string f, msg;
            // Block until broadcast arrives
            if (R.subReceive(f, msg, -1)) {
                got[rid] = msg;
            }
        });
    }

    // Publisher
    Communicator P{senderId, BASE_PORT, "127.0.0.1", num_parties};
    P.setUpPublisher();
    // Let SUB connects settle briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(P.pubBroadcast(payload));

    for (auto& t : rxs) if (t.joinable()) t.join();

    for (int rid = 2; rid <= num_parties; ++rid) {
        EXPECT_EQ(got[rid], payload) << "receiver id=" << rid;
    }
}

TEST(CommunicatorTest, TimingOfPubBroadcastAcrossPartyCounts) {
    using clock = std::chrono::steady_clock;
    const std::vector<int> party_counts = {2, 4, 6, 8, 10};
    const size_t payload_size = 1024 * 1024; // 1MB
    const int iterations_warmup = 5;
    const int iterations = 1000; // match dealer parallel test for comparability

    for (int num_parties : party_counts) {
        const int senderId = 1;
        const int num_receivers = num_parties - 1;

        std::string payload(payload_size, 'b');

        // Publisher: PUB for broadcast + ROUTER to receive ACKs
        Communicator P{senderId, BASE_PORT, "127.0.0.1", num_parties};
        P.setUpPublisher();
        P.setUpRouter();

        // Receivers: SUB to receive broadcasts, DEALER to send ACKs to publisher's ROUTER
        std::vector<std::thread> rxs;
        for (int rid = 2; rid <= num_parties; ++rid) {
            rxs.emplace_back([&, rid]() {
                Communicator R{rid, BASE_PORT, "127.0.0.1", num_parties};
                R.setUpSubscribers();
                R.setUpPerPeerDealers();
                std::string f, msg;
                const int total_rounds = iterations_warmup + iterations;
                for (int i = 0; i < total_rounds; ++i) {
                    if (!R.subReceive(f, msg, -1)) break;
                    (void)R.dealerSendTo(senderId, "a");
                }
            });
        }

        // Let SUB connects settle to avoid slow-joiner drops
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Warmup rounds
        for (int w = 0; w < iterations_warmup; ++w) {
            ASSERT_TRUE(P.pubBroadcast(payload));
            for (int k = 0; k < num_receivers; ++k) {
                std::string fromAck, ack;
                ASSERT_TRUE(P.routerReceive(fromAck, ack, -1));
            }
        }

        // Timed rounds
        std::string fromAck, ack;
        auto start = clock::now();
        for (int it = 0; it < iterations; ++it) {
            P.pubBroadcast(payload);
            for (int k = 0; k < num_receivers; ++k) {
                P.routerReceive(fromAck, ack, -1);
            }
        }
        auto end = clock::now();

        const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        const double avg_ms_per_round = (static_cast<double>(duration_ns) / iterations) / 1e6;
        std::cout << "[PUB] Parties " << num_parties
                  << ": avg round (broadcast+ACKs) = " << avg_ms_per_round
                  << " ms (payload 1MB)" << std::endl;

        for (auto& t : rxs) if (t.joinable()) t.join();
    }
}
