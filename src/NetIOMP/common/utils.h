#ifndef EMP_UTILS_H__
#define EMP_UTILS_H__

#include <iostream>
#include <cstdlib>

namespace emp {

inline void parse_party_and_port(char** argv, int* party, int* port) {
    if (argv[1] == nullptr || argv[2] == nullptr) {
        std::cerr << "Usage: <executable> <party_id> <port>" << std::endl;
        exit(1);
    }
    *party = atoi(argv[1]);
    *port = atoi(argv[2]);
}

} // namespace emp
#endif // EMP_UTILS_H__