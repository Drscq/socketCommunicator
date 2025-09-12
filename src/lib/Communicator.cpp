#include <thread>
#include <vector>
#include <numeric>
#include "Communicator.h"
#include <iostream>



// Send the payload to all peer ROUTERs in parallel (one thread per peer, each with its own DEALER socket)
bool Communicator::dealerSendToAllParallel(const std::string& payload) {
    // Ensure results vector is sized (lazy safeguard in case constructor variant without num_parties was used)
    if (workerResults_.size() < static_cast<size_t>(num_parties)) {
        workerResults_.assign(static_cast<size_t>(num_parties), 0);
    }

    for (size_t i = 0; i < num_parties; ++i) {
        const int peerId = this->ids[i];
        if (peerId == this->id) continue; // skip self
        // Reset slot before starting worker
        workerResults_[i] = 0;
        workerThreads_.emplace_back([this, &payload, i, peerId]() {
            // Use pre-initialized per-peer DEALER sockets; no connect here.
            workerResults_[i] = this->dealerSendTo(peerId, payload) ? 1 : 0;
        });
    }

    for (auto& t : workerThreads_) if (t.joinable()) t.join();
    workerThreads_.clear();

    // Return true only if all non-self sends succeeded
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] == this->id) continue;
        if (workerResults_[i] == 0) return false;
    }
    return true;
}

Communicator::Communicator(int id, int port_base, std::string address)
    : id(id), port_base(port_base), address(address) {}

Communicator::Communicator(int id, int port_base, std::string address, int num_parties)
    : id(id), port_base(port_base), address(address), num_parties(num_parties) {
        ids.reserve(num_parties);
        for (int i = 1; i <= num_parties; ++i) {
            ids.push_back(i);
        }
    // Pre-size reusable worker structures once
    workerResults_.assign(static_cast<size_t>(num_parties), 0);
    workerThreads_.reserve(num_parties);
    }

Communicator::~Communicator() {
    joinAndClearWorkers();
}

void Communicator::joinAndClearWorkers() noexcept {
    for (auto& t : workerThreads_) {
        if (t.joinable()) {
            try { t.join(); } catch (...) { /* swallow */ }
        }
    }
    workerThreads_.clear();
}


void Communicator::setUpRouter() {
    if (!context_) {
        context_ = std::make_unique<zmq::context_t>(1);
    }
    if (!router_) {
        router_ = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::router);
        std::string bind_address = "tcp://" + address + ":" + std::to_string(port_base + id);
        router_->bind(bind_address);
        router_->set(zmq::sockopt::rcvhwm, 0); // no limit
        router_->set(zmq::sockopt::rcvtimeo, -1); // block indefinitely
        // std::cout << "Router " << id << " running at " << bind_address << std::endl;
    }
}

void Communicator::setUpPerPeerDealers() {
    if (!context_) {
        context_ = std::make_unique<zmq::context_t>(1);
    }
    for (int party_id : this->ids) {
        if (party_id == this->id) continue; // skip self
        auto& sockPtr = perPeerDealer_[party_id];
        if (sockPtr) continue; // already prepared
        const std::string addr = "tcp://" + address + ":" + std::to_string(port_base + party_id);
        sockPtr = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::dealer);
        const std::string plainId = std::to_string(this->id);
        sockPtr->set(zmq::sockopt::routing_id, plainId);
        // Be tolerant but avoid indefinite blocks on send
        sockPtr->set(zmq::sockopt::sndtimeo, 1000);
        sockPtr->set(zmq::sockopt::sndhwm, 0); // no limit
        sockPtr->connect(addr);
        // std::cout << "Dealer " << this->id << " (per-peer) connected to " << addr << std::endl;
    }
}

void Communicator::setUpRouterDealer() {
    this->setUpRouter();
    this->setUpPerPeerDealers();
}

void Communicator::setUpPublisher() {
    if (!context_) {
        context_ = std::make_unique<zmq::context_t>(1);
    }
    if (!pub_) {
        pub_ = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::pub);
        std::string bind_address = "tcp://" + address + ":" + std::to_string(port_base + 1000 + id);
        pub_->bind(bind_address);
        pub_->set(zmq::sockopt::sndhwm, 0); // no limit
        // PUB is best-effort; leave sndtimeo default, we use dontwait on send
    }
}

void Communicator::setUpSubscribers() {
    if (!context_) {
        context_ = std::make_unique<zmq::context_t>(1);
    }
    if (!sub_) {
        sub_ = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::sub);
        // Subscribe to all topics
        sub_->set(zmq::sockopt::subscribe, "");
        sub_->set(zmq::sockopt::rcvhwm, 0);
        sub_->set(zmq::sockopt::rcvtimeo, -1);
    }
    for (int party_id : this->ids) {
        if (party_id == this->id) continue;
        std::string addr = "tcp://" + address + ":" + std::to_string(port_base + 1000 + party_id);
        // ZeroMQ allows duplicate connects; we don't track to keep overhead minimal.
        sub_->connect(addr);
    }
}

// bool Communicator::dealerSendToAll(const std::string& payload) {
//     if (!dealer_) return false;
//     zmq::message_t msg(payload.begin(), payload.end());
//     auto rc = dealer_->send(msg, zmq::send_flags::none);
//     return rc.has_value();
// }

bool Communicator::routerReceive(std::string& fromIdentity, std::string& payload, int timeoutMs) {
    if (!router_) return false;
    // Use socket receive timeout instead of poll to keep it simple and robust
    if (timeoutMs >= 0) router_->set(zmq::sockopt::rcvtimeo, timeoutMs);
    // ROUTER sockets receive [identity][payload]
    zmq::message_t identity;
    zmq::message_t payloadMsg;

    // Some patterns insert an empty delimiter; handle both 2- or 3-part messages
    if (!router_->recv(identity, zmq::recv_flags::none)) return false;

    // Peek if there is a second frame
    zmq::message_t second;
    auto secondRes = router_->recv(second, zmq::recv_flags::none);
    if (!secondRes.has_value()) return false;

    // Common patterns:
    // 1) [id][payload]
    // 2) [id][empty][payload]
    int more = 0;
    size_t more_size = sizeof(more);
    router_->getsockopt(ZMQ_RCVMORE, &more, &more_size);

    if (more && second.size() == 0) {
        // Empty delimiter present; next frame is payload
    auto payloadRes = router_->recv(payloadMsg, zmq::recv_flags::none);
    if (!payloadRes.has_value()) return false;
    } else {
        // No delimiter; 'second' is payload
        payloadMsg = std::move(second);
    }

    fromIdentity = identity.to_string();
    payload.assign(static_cast<const char*>(payloadMsg.data()), payloadMsg.size());
    return true;
}

bool Communicator::routerSend(const std::string& toIdentity, const std::string& payload) {
    if (!router_) return false;
    // ROUTER send multipart: [identity][payload] (no delimiter)
    zmq::message_t idFrame(toIdentity.begin(), toIdentity.end());
    zmq::message_t payloadFrame(payload.begin(), payload.end());

    auto s1 = router_->send(idFrame, zmq::send_flags::sndmore);
    if (!s1.has_value()) return false;
    auto s2 = router_->send(payloadFrame, zmq::send_flags::none);
    return s2.has_value();
}

// bool Communicator::dealerReceive(std::string& payload, int timeoutMs) {
//     if (!dealer_) return false;
//     dealer_->set(zmq::sockopt::rcvtimeo, timeoutMs);

//     // Receive all frames of the message, skip empties, return last non-empty as payload
//     std::vector<zmq::message_t> frames;
//     while (true) {
//         zmq::message_t frame;
//         auto r = dealer_->recv(frame, zmq::recv_flags::none);
//         if (!r.has_value()) return false;
//         frames.push_back(std::move(frame));
//         int more = 0;
//         size_t more_size = sizeof(more);
//         dealer_->getsockopt(ZMQ_RCVMORE, &more, &more_size);
//         if (!more) break;
//     }
//     for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
//         if (it->size() > 0) {
//             payload.assign(static_cast<const char*>(it->data()), it->size());
//             return true;
//         }
//     }
//     payload.clear();
//     return true;
// }

bool Communicator::dealerSendTo(int peerId, const std::string& payload) {
    if (peerId == this->id) return false;

    auto it = perPeerDealer_.find(peerId);
    if (it == perPeerDealer_.end() || !it->second) return false; // not prepared
    auto& sockPtr = it->second;

    zmq::message_t msg(payload.begin(), payload.end());
    auto rc = sockPtr->send(msg, zmq::send_flags::dontwait);
    if (rc.has_value()) return true;
    return false;
}

bool Communicator::dealerSendTo(int peerId, zmq::message_t&& payload) {
    if (peerId == this->id) return false;

    auto it = perPeerDealer_.find(peerId);
    if (it == perPeerDealer_.end() || !it->second) return false; // not prepared
    auto& sockPtr = it->second;

    auto rc = sockPtr->send(std::move(payload), zmq::send_flags::dontwait);
    if (rc.has_value()) return true;
    return false;
}

bool Communicator::pubBroadcast(const std::string& payload) {
    if (!pub_) return false;
    zmq::message_t data(payload.begin(), payload.end());
    auto s = pub_->send(data, zmq::send_flags::dontwait);
    return s.has_value();
}

bool Communicator::subReceive(std::string& fromPublisherId, std::string& payload, int timeoutMs) {
    if (!sub_) return false;
    if (timeoutMs >= 0) sub_->set(zmq::sockopt::rcvtimeo, timeoutMs);
    zmq::message_t data;
    auto r = sub_->recv(data, zmq::recv_flags::none);
    if (!r.has_value()) return false;
    fromPublisherId.clear();
    payload.assign(static_cast<const char*>(data.data()), data.size());
    return true;
}