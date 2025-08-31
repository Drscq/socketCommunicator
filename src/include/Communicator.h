#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <string>

class Communicator {
public:
    Communicator(int id, int port_base, std::string address = "localhost");
    ~Communicator();

    // Accessors for testing and usage
    int getId() const noexcept { return id; }
    int getPortBase() const noexcept { return port_base; }
    const std::string& getAddress() const noexcept { return address; }

private:
    int id;
    int port_base;
    std::string address;
};

#endif // COMMUNICATOR_H