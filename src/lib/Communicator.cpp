#include "Communicator.h"
#include <iostream>

Communicator::Communicator(int id, int port_base, std::string address)
    : id(id), port_base(port_base), address(address) {}

Communicator::~Communicator() {}


void Communicator::setUpRouter() {
    if (!context_) {
        context_ = std::make_unique<zmq::context_t>(1);
    }
    if (!router_) {
        router_ = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::router);
        std::string bind_address = "tcp://" + address + ":" + std::to_string(port_base + id);
        router_->bind(bind_address);
        std::cout << "Router " << id << " running at " << bind_address << std::endl;
    }
}


void Communicator::setUpDealer(std::vector<int> party_list) {
    if (!context_) {
        context_ = std::make_unique<zmq::context_t>(1);
    }
    if (!dealer_) {
        dealer_ = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::dealer);
        // Dealer identity should be our id
        dealer_->set(zmq::sockopt::routing_id, std::to_string(this->id));
    }
    for (auto& party_id : party_list) {
        if (party_id == this->id) continue; // skip self
        std::string connect_address = "tcp://" + address + ":" + std::to_string(port_base + party_id);
        dealer_->connect(connect_address);
        std::cout << "Dealer " << this->id << " connected to " << connect_address << std::endl;
    }
}

bool Communicator::dealerSend(const std::string& payload) {
    if (!dealer_) return false;
    zmq::message_t msg(payload.begin(), payload.end());
    auto rc = dealer_->send(msg, zmq::send_flags::none);
    return rc.has_value();
}

bool Communicator::routerReceive(std::string& fromIdentity, std::string& payload, int timeoutMs) {
    if (!router_) return false;
    // Poll once for readability with timeout
    zmq::pollitem_t items[] = { { static_cast<void*>(*router_), 0, ZMQ_POLLIN, 0 } };
    zmq::poll(items, 1, std::chrono::milliseconds(timeoutMs));
    if (!(items[0].revents & ZMQ_POLLIN)) {
        return false; // timeout
    }

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

bool Communicator::dealerReceive(std::string& payload, int timeoutMs) {
    if (!dealer_) return false;
    zmq::pollitem_t items[] = { { static_cast<void*>(*dealer_), 0, ZMQ_POLLIN, 0 } };
    zmq::poll(items, 1, std::chrono::milliseconds(timeoutMs));
    if (!(items[0].revents & ZMQ_POLLIN)) return false;

    // Receive all frames of the message, skip empties, return last non-empty as payload
    std::vector<zmq::message_t> frames;
    while (true) {
        zmq::message_t frame;
        auto r = dealer_->recv(frame, zmq::recv_flags::none);
        if (!r.has_value()) return false;
        frames.push_back(std::move(frame));
        int more = 0;
        size_t more_size = sizeof(more);
        dealer_->getsockopt(ZMQ_RCVMORE, &more, &more_size);
        if (!more) break;
    }
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
        if (it->size() > 0) {
            payload.assign(static_cast<const char*>(it->data()), it->size());
            return true;
        }
    }
    payload.clear();
    return true;
}

bool Communicator::dealerSendTo(int peerId, const std::string& payload) {
    if (peerId == this->id) return false;
    if (!context_) {
        context_ = std::make_unique<zmq::context_t>(1);
    }

    auto& sockPtr = perPeerDealer_[peerId];
    if (!sockPtr) {
        sockPtr = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::dealer);
        sockPtr->set(zmq::sockopt::routing_id, std::to_string(this->id));
        const std::string addr = "tcp://" + address + ":" + std::to_string(port_base + peerId);
        sockPtr->connect(addr);
        std::cout << "Dealer " << this->id << " (dedicated) connected to " << addr << std::endl;
    }

    zmq::message_t msg(payload.begin(), payload.end());
    auto rc = sockPtr->send(msg, zmq::send_flags::none);
    return rc.has_value();
}