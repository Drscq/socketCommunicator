#include "Communicator.h"
#include <iostream>

Communicator::Communicator(int id, int port_base, std::string address)
    : id(id), port_base(port_base), address(address) {}

Communicator::~Communicator() {}


void Communicator::setUpRouter() {
    zmq::context_t context(1);
    zmq::socket_t router(context, zmq::socket_type::router);
    std::string bind_address = "tcp://" + address + ":" + std::to_string(port_base + id);
    router.bind(bind_address);
    std::cout << "Router " << id << " running at " << bind_address << std::endl;

    // while (true) {
    //     // ROUTER sockets receive [identity][empty][payload]
    //     zmq::message_t identity;
    //     zmq::message_t empty;
    //     zmq::message_t payload;

    //     router.recv(identity, zmq::recv_flags::none);
    //     router.recv(empty, zmq::recv_flags::none);
    //     router.recv(payload, zmq::recv_flags::none);

    //     std::cout << "Router " << id << " received message from " << identity.to_string() << std::endl;
    // }
}


void Communicator::setUpDealer(std::vector<int> party_list) {
    zmq::context_t context(1);
    zmq::socket_t dealer(context, zmq::socket_type::dealer);
    for (auto& party_id : party_list) {
        if (party_id == this->id) continue; // skip self
        std::string connect_address = "tcp://" + address + ":" + std::to_string(port_base + party_id);
        // routing_id expects a string identifier; convert the integer id to a string
        dealer.set(zmq::sockopt::routing_id, std::to_string(party_id));
        dealer.connect(connect_address);
        std::cout << "Dealer " << this->id << " connected to " << connect_address << std::endl;
    }
}