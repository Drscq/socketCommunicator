#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <unordered_map>
#include <zmq.hpp>
#include <cstdint>
class Communicator {
public:
    Communicator(int id, int port_base, std::string address);
    Communicator(int id, int port_base, std::string address, int num_parties);
    ~Communicator();

    // Accessors for testing and usage
    int getId() const noexcept { return id; }
    int getPortBase() const noexcept { return port_base; }
    const std::string& getAddress() const noexcept { return address; }

    // run router or dealer mod
    void setUpRouter();
    // Prepare dedicated per-peer DEALER sockets (one per peer) and connect to their ROUTERs.
    // Call this after all routers are bound. Skips self.
    void setUpPerPeerDealers();
    void setUpRouterDealer();

    // Messaging API
    // Dealer sends a single-frame payload to connected router(s).
    // Returns true on success, false on failure.
    bool dealerSendToAll(const std::string& payload);

    // Send the payload to all peer ROUTERs in parallel (one thread per peer, each with its own DEALER socket)
    bool dealerSendToAllParallel(const std::string& payload);

    // Router receives one message in form [identity][payload].
    // If timeoutMs < 0, block until a message arrives; otherwise wait up to timeoutMs.
    bool routerReceive(std::string& fromIdentity, std::string& payload, int timeoutMs = -1);

    // Router sends a single-frame payload to a specific dealer identity.
    bool routerSend(const std::string& toIdentity, const std::string& payload);

    // Dealer receives one message payload from router.
    bool dealerReceive(std::string& payload, int timeoutMs = 1000);

    // Dealer sends to a specific peer's router using a dedicated per-peer DEALER socket.
    bool dealerSendTo(int peerId, const std::string& payload);

private:
    int id;
    int port_base;
    std::string address;
    int num_parties = 0; // optional, for informational purposes

    // Persistent ZeroMQ context and sockets (created on demand)
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> router_;
    std::unique_ptr<zmq::socket_t> dealer_;

    // Optional: dedicated DEALER sockets per peer for targeted sends.
    std::unordered_map<int, std::unique_ptr<zmq::socket_t>> perPeerDealer_;

    // Reusable worker resources for parallel dealer sends
    std::vector<std::thread> workerThreads_;
    // Use a byte vector instead of vector<bool> to avoid bit-packing data races
    std::vector<uint8_t> workerResults_;
    void joinAndClearWorkers() noexcept;

    std::vector<int> ids;
};

#endif // COMMUNICATOR_H