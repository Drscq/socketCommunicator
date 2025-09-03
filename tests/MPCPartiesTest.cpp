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

// // A simple two-party exchange where A sends 5 to B and B sends 6 to A.
// // Each party computes the sum and verifies it's 11.
// TEST(MPCPartiesTest, TwoPartyExchangeSumIs11) {
//     const int base = 12000; // isolate ports from other tests
//     const std::string host = "127.0.0.1";

//     // Party A (id=1) and Party B (id=2)
//     Communicator A{1, base, host};
//     Communicator B{2, base, host};

//     // Each plays both roles: bind router and prepare dealer
//     A.setUpRouter();
//     B.setUpRouter();
//     A.setUpDealer({1,2});
//     B.setUpDealer({1,2});

//     // Small delay to allow connections
//     std::this_thread::sleep_for(std::chrono::milliseconds(100));

//     int numRounds = 10; 
//     for (int i = 0; i < numRounds; ++i) {
//         // A -> B: send value 5
//         ASSERT_TRUE(A.dealerSendToAll("5"));
//         std::string fromB, recvB;
//     ASSERT_TRUE(B.routerReceive(fromB, recvB));
//         EXPECT_EQ(fromB, std::to_string(1));
//         EXPECT_EQ(recvB, "5");

//         // B -> A: send value 6 back
//         ASSERT_TRUE(B.dealerSendTo(1, "6"));
//         std::string recvA;
//     ASSERT_TRUE(A.routerReceive(fromB, recvA));
//         EXPECT_EQ(recvA, "6");

//         // Compute sums at both parties
//         int a_local = 5;
//         int a_peer = std::stoi(recvA);
//         int b_local = 6;
//         int b_peer = std::stoi(recvB);

//         EXPECT_EQ(a_local + a_peer, 11);
//         EXPECT_EQ(b_local + b_peer, 11);
//     }
// }


// // Performance comparison: sequential per-peer send vs parallel send-to-all
// TEST(MPCPartiesTest, BroadcastPerformance) {
//     const int N = 5;               // number of parties (>= 2)
//     const int base = 14000;        // isolated port base
//     const std::string host = "127.0.0.1";
//     const std::vector<size_t> sizes = {64, 4096, 65536}; // payload sizes in bytes

//     auto make_payload = [](size_t n) {
//         std::string s;
//         s.resize(n, 'x');
//         return s;
//     };

//     std::vector<int> ids; ids.reserve(N); for (int i = 1; i <= N; ++i) ids.push_back(i);

//     for (size_t si = 0; si < sizes.size(); ++si) {
//         // Fresh setup per size for cleaner measurements
//         std::vector<std::unique_ptr<Communicator>> parties;
//         parties.reserve(N);
//         for (int i = 1; i <= N; ++i) parties.emplace_back(std::make_unique<Communicator>(i, base, host));
//         for (int i = 0; i < N; ++i) parties[i]->setUpRouter();
//         for (int i = 0; i < N; ++i) parties[i]->setUpDealer(ids);
//         std::this_thread::sleep_for(std::chrono::milliseconds(200));

//         const std::string payload = make_payload(sizes[si]);
//         const int senderIdx = 0; // party id=1 as sender

//         // Drain helper for one message from each receiver (from sender id)
//     auto drain_from_all = [&](int expectedCount) {
//             int drained = 0;
//             for (int j = 0; j < N; ++j) {
//                 if (j == senderIdx) continue;
//                 std::string from, data;
//         ASSERT_TRUE(parties[j]->routerReceive(from, data)) << "receiver " << (j+1) << " blocked";
//                 // minimal sanity checks
//                 EXPECT_EQ(from, std::to_string(senderIdx + 1));
//                 EXPECT_EQ(data.size(), payload.size());
//                 drained++;
//             }
//             EXPECT_EQ(drained, expectedCount);
//         };

//         // Sequential: per-peer send in a loop using dealerSendTo
//         {
//             auto t0 = std::chrono::steady_clock::now();
//             for (int peerId = 1; peerId <= N; ++peerId) {
//                 if (peerId == senderIdx + 1) continue;
//                 ASSERT_TRUE(parties[senderIdx]->dealerSendTo(peerId, payload));
//             }
//             // Drain one message at each receiver
//             drain_from_all(N - 1);
//             auto t1 = std::chrono::steady_clock::now();
//             auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
//             std::cout << "Sequential broadcast payload=" << payload.size() << "B took " << us << " us\n";
//         }

//         // Parallel: send to all using dealerSendToAllParallel
//         {
//             auto t0 = std::chrono::steady_clock::now();
//             ASSERT_TRUE(parties[senderIdx]->dealerSendToAllParallel(payload, ids));
//             drain_from_all(N - 1);
//             auto t1 = std::chrono::steady_clock::now();
//             auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
//             std::cout << "Parallel broadcast payload=" << payload.size() << "B took " << us << " us\n";
//         }
//     }
// }

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


