#include <gtest/gtest.h>
#include "Communicator.h"

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
