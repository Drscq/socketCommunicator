#include <gtest/gtest.h>
#include "Communicator.h"
#include <thread>
#include <chrono>
#include <vector>

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
