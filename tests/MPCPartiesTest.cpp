#include <gtest/gtest.h>
#include "Communicator.h"
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <numeric>

// A simple two-party exchange where A sends 5 to B and B sends 6 to A.
// Each party computes the sum and verifies it's 11.
TEST(MPCPartiesTest, TwoPartyExchangeSumIs11) {
    const int base = 12000; // isolate ports from other tests
    const std::string host = "127.0.0.1";

    // Party A (id=1) and Party B (id=2)
    Communicator A{1, base, host};
    Communicator B{2, base, host};

    // Each plays both roles: bind router and prepare dealer
    A.setUpRouter();
    B.setUpRouter();
    A.setUpDealer({1,2});
    B.setUpDealer({1,2});

    // Small delay to allow connections
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int numRounds = 10; 
    for (int i = 0; i < numRounds; ++i) {
        // A -> B: send value 5
        ASSERT_TRUE(A.dealerSend("5"));
        std::string fromB, recvB;
        ASSERT_TRUE(B.routerReceive(fromB, recvB, 1000));
        EXPECT_EQ(fromB, std::to_string(1));
        EXPECT_EQ(recvB, "5");

        // B -> A: send value 6 back
        ASSERT_TRUE(B.dealerSendTo(1, "6"));
        std::string recvA;
        ASSERT_TRUE(A.routerReceive(fromB, recvA, 1000));
        EXPECT_EQ(recvA, "6");

        // Compute sums at both parties
        int a_local = 5;
        int a_peer = std::stoi(recvA);
        int b_local = 6;
        int b_peer = std::stoi(recvB);

        EXPECT_EQ(a_local + a_peer, 11);
        EXPECT_EQ(b_local + b_peer, 11);
    }
}

// N-party all-to-all sum: each party i holds value v[i], sends to every other party.
// Each party computes the total sum locally and checks against expected.
TEST(MPCPartiesTest, NPartyAllToAllSum) {
    const int N = 4; // adjust this to change number of parties
    const int base = 13000; // separate from other tests' ports
    const std::string host = "127.0.0.1";

    // Prepare parties 1..N
    std::vector<std::unique_ptr<Communicator>> parties;
    parties.reserve(N);
    for (int i = 1; i <= N; ++i) {
        parties.emplace_back(std::make_unique<Communicator>(i, base, host));
    }

    // Routers and dealers
    for (int i = 0; i < N; ++i) parties[i]->setUpRouter();
    std::vector<int> ids; ids.reserve(N); for (int i = 1; i <= N; ++i) ids.push_back(i);
    for (int i = 0; i < N; ++i) parties[i]->setUpDealer(ids);

    // Allow connections to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Assign values v[i] (1..N for determinism)
    std::vector<int> values(N);
    for (int i = 0; i < N; ++i) values[i] = i + 1;
    const int expected_sum = std::accumulate(values.begin(), values.end(), 0);

    // Each party sends its value to every other party
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (i == j) continue;
            ASSERT_TRUE(parties[i]->dealerSendTo(j + 1, std::to_string(values[i])));
        }
    }

    // Each party receives N-1 messages and computes local total
    for (int j = 0; j < N; ++j) {
        int sum_peer = 0;
        for (int k = 0; k < N - 1; ++k) {
            std::string fromId, payload;
            ASSERT_TRUE(parties[j]->routerReceive(fromId, payload, 2000)) << "party " << (j+1) << " timed out on recv " << k;
            sum_peer += std::stoi(payload);
        }
        const int total = values[j] + sum_peer;
        EXPECT_EQ(total, expected_sum) << "party " << (j+1) << " computed wrong total";
    }
}


