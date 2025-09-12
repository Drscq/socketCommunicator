#ifndef CMPC_CONFIG_H__
#define CMPC_CONFIG_H__

const static int nP = 12;
#define LOCALHOST
#ifdef LOCALHOST
const static char* IP[nP+1] = {"", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1"};
#else
//Fill your IP addresses here
const static char* IP[nP+1] = {"", "10.11.100.101", "10.11.100.102", "10.11.100.103"};
#endif
const bool lan_network = false;

#endif //CMPC_CONFIG_H__