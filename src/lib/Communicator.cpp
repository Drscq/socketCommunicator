#include "Communicator.h"


Communicator::Communicator(int id, int port_base, std::string address)
    : id(id), port_base(port_base), address(address) {}

Communicator::~Communicator() {}