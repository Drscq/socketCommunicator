#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <string>
#include <vector>
#include <memory>
#include <zmq.hpp>
class Communicator {
public:
    Communicator(int id, int port_base, std::string address = "localhost");
    ~Communicator();

    // Accessors for testing and usage
    int getId() const noexcept { return id; }
    int getPortBase() const noexcept { return port_base; }
    const std::string& getAddress() const noexcept { return address; }

    // run router or dealer mod
    void setUpRouter();
    void setUpDealer(std::vector<int> party_list);

    // Messaging API
    // Dealer sends a single-frame payload to connected router(s).
    // Returns true on success, false on failure.
    bool dealerSend(const std::string& payload);

    // Router receives one message in form [identity][payload].
    // Returns true if a message was received before timeoutMs, false otherwise.
    bool routerReceive(std::string& fromIdentity, std::string& payload, int timeoutMs = 1000);

private:
    int id;
    int port_base;
    std::string address;

    // Persistent ZeroMQ context and sockets (created on demand)
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> router_;
    std::unique_ptr<zmq::socket_t> dealer_;
};

#endif // COMMUNICATOR_H