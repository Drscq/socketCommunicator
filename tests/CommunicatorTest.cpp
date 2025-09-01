#include <gtest/gtest.h>
#include "Communicator.h"
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <unordered_set>

TEST(CommunicatorTest, ConstructorStoresValues) {
    Communicator c{42, 5000, "192.168.1.10"};
    EXPECT_EQ(c.getId(), 42);
    EXPECT_EQ(c.getPortBase(), 5000);
    EXPECT_EQ(c.getAddress(), std::string("192.168.1.10"));
}

TEST(CommunicatorTest, DefaultAddressIsLocalhost) {
    Communicator c{1, 8080};
    EXPECT_EQ(c.getAddress(), std::string("localhost"));
}

TEST(CommunicatorTest, SetUpRouterDoesNotThrow) {
    // Test that setUpRouter can be called without throwing exceptions
    Communicator c{1, 9000, "127.0.0.1"};
    EXPECT_NO_THROW(c.setUpRouter());
}

TEST(CommunicatorTest, SetUpDealerWithEmptyListDoesNotThrow) {
    // Test that setUpDealer with empty party list doesn't throw
    Communicator c{1, 9100, "127.0.0.1"};
    std::vector<int> empty_list;
    EXPECT_NO_THROW(c.setUpDealer(empty_list));
}

TEST(CommunicatorTest, SetUpDealerWithSelfOnlyDoesNotThrow) {
    // Test that setUpDealer with only self in party list doesn't throw
    Communicator c{1, 9200, "127.0.0.1"};
    std::vector<int> self_only{1};
    EXPECT_NO_THROW(c.setUpDealer(self_only));
}

TEST(CommunicatorTest, SetUpDealerWithMultiplePartiesDoesNotThrow) {
    // Test that setUpDealer with multiple parties doesn't throw
    // Note: This test may fail if the router endpoints aren't available
    // but it tests the method signature and basic functionality
    Communicator c{1, 9300, "127.0.0.1"};
    std::vector<int> party_list{1, 2, 3};
    
    // For now, just test that the method can be called
    // In a real scenario, you'd want to set up actual routers first
    EXPECT_NO_THROW(c.setUpDealer(party_list));
}

TEST(CommunicatorTest, IntegrationTestRouterDealerSetup) {
    // Integration test showing router and dealer setup work together
    // This test demonstrates the intended usage pattern
    
    // Set up router for party 1
    Communicator router_comm{1, 9400, "127.0.0.1"};
    
    // Set up dealer for party 2 that will connect to party 1
    Communicator dealer_comm{2, 9400, "127.0.0.1"};
    std::vector<int> party_list{1, 2}; // Party 2 connects to party 1
    
    // Both should set up without throwing
    EXPECT_NO_THROW(router_comm.setUpRouter());
    EXPECT_NO_THROW(dealer_comm.setUpDealer(party_list));
}

TEST(CommunicatorTest, DealerSendRouterReceive) {
    // Set up router (id=1) and dealer (id=2) on same address/port base
    Communicator router{1, 9500, "127.0.0.1"};
    Communicator dealer{2, 9500, "127.0.0.1"};

    router.setUpRouter();
    dealer.setUpDealer({1, 2});

    // Give a tiny moment for connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send from dealer and receive at router
    ASSERT_TRUE(dealer.dealerSend("hello"));

    std::string from;
    std::string msg;
    ASSERT_TRUE(router.routerReceive(from, msg, 1000));
    EXPECT_EQ(from, std::to_string(2));
    EXPECT_EQ(msg, "hello");
}

TEST(CommunicatorTest, TwoPartiesBidirectionalManyRounds) {
    // Party A: id=1, Party B: id=2
    Communicator A{1, 9600, "127.0.0.1"};
    Communicator B{2, 9600, "127.0.0.1"};

    // Setup
    A.setUpRouter();
    B.setUpRouter();
    A.setUpDealer({1,2}); // A connects to B's router
    B.setUpDealer({1,2}); // B connects to A's router

    // Allow brief time for connections
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const int rounds = 5;
    for (int r = 0; r < rounds; ++r) {
        // A -> B
        ASSERT_TRUE(A.dealerSend("ping-" + std::to_string(r)));
        std::string from, msg;
        ASSERT_TRUE(B.routerReceive(from, msg, 1000));
        EXPECT_EQ(from, std::to_string(1));
        EXPECT_EQ(msg, "ping-" + std::to_string(r));

        // B responds to A using routerSend (addressed by A's identity)
        ASSERT_TRUE(B.routerSend(std::to_string(1), "pong-" + std::to_string(r)));
        std::string recv;
        ASSERT_TRUE(A.dealerReceive(recv, 1000));
        EXPECT_EQ(recv, "pong-" + std::to_string(r));
    }
}

TEST(CommunicatorTest, FivePartiesFullMeshRoundRobinTwoCycles) {
    // IDs 1..5 on same base
    const int base = 9700;
    const std::string host = "127.0.0.1";
    const std::vector<int> ids{1,2,3,4,5};

    // Create parties
    std::vector<std::unique_ptr<Communicator>> parties;
    parties.reserve(ids.size());
    for (int id : ids) {
        parties.emplace_back(std::make_unique<Communicator>(id, base, host));
    }

    // Routers and dealers
    for (auto& p : parties) p->setUpRouter();
    for (auto& p : parties) p->setUpDealer(ids);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Each dealer sends 8 messages (2 full round-robin cycles over 4 peers)
    const int cycles = 2;     // expect 2 messages per peer
    const int perCycle = 4;   // 4 peers
    for (int c = 0; c < cycles; ++c) {
        for (int m = 0; m < perCycle; ++m) {
            for (size_t i = 0; i < parties.size(); ++i) {
                const int fromId = ids[i];
                ASSERT_TRUE(parties[i]->dealerSend("m-" + std::to_string(fromId) + "-" + std::to_string(c) + "-" + std::to_string(m)));
            }
        }
    }

    // Collect on routers until each router has received exactly 'cycles' messages from each other id
    std::unordered_map<int, std::unordered_map<int,int>> recvCounts; // routerId -> (fromId -> count)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    size_t satisfiedRouters = 0;
    std::unordered_set<int> satisfied;

    while (std::chrono::steady_clock::now() < deadline) {
        bool any = false;
        for (size_t j = 0; j < parties.size(); ++j) {
            const int routerId = ids[j];
            std::string from, payload;
            if (parties[j]->routerReceive(from, payload, 5)) {
                any = true;
                int fromId = std::stoi(from);
                if (fromId != routerId) {
                    recvCounts[routerId][fromId]++;
                }
            }

            // Check satisfaction for this router
            bool ok = true;
            for (int other : ids) {
                if (other == routerId) continue;
                if (recvCounts[routerId][other] != cycles) { ok = false; break; }
            }
            if (ok && !satisfied.count(routerId)) {
                satisfied.insert(routerId);
            }
        }
        if (satisfied.size() == ids.size()) break;
        if (!any) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Assert all routers satisfied
    for (int routerId : ids) {
        for (int other : ids) {
            if (other == routerId) continue;
            ASSERT_EQ(recvCounts[routerId][other], cycles) << "router " << routerId << " from " << other;
        }
    }
}

TEST(CommunicatorTest, DealerSendToTargetsSpecificPeer) {
    const int base = 9900;
    Communicator A{1, base, "127.0.0.1"};
    Communicator B{2, base, "127.0.0.1"};
    Communicator C{3, base, "127.0.0.1"};

    A.setUpRouter();
    B.setUpRouter();
    C.setUpRouter();

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
