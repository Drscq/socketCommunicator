#include "common/utils.h"
#include "netmp.h"
#include <iostream>

using namespace std;

int main(int argc, char** argv) {
	int port, party;
	emp::parse_party_and_port(argv, &party, &port);

	NetIOMP<3> io(party, port);
    cout << "Party " << party << " initialized." << endl;

	for(int i = 1; i <= 3; ++i) {
        for(int j = i + 1; j <= 3; ++j) {
            if(i == party) {
                int data = i * 100 + j;
                cout << "Party " << party << " sending " << data << " to party " << j << endl;
                io.send_data(j, &data, sizeof(int));
            } else if (j == party) {
                int data = 0;
                io.recv_data(i, &data, sizeof(int));
                cout << "Party " << party << " received " << data << " from party " << i << endl;
                if(data != i * 100 + j) {
                    cout << "WRONG DATA!" << endl;
                }
            }
        }
    }
	io.flush();
    cout << "Party " << party << " finished." << endl;

	return 0;
}