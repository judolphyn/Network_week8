#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <thread>
#include <set>
using namespace std;

set<int> sds;

void usage() {
	cout << "syntax: echo-server <port> [-e[-b]]\n";
	cout << "sample: echo-server 1234 -e -b\n";
}

struct Param {
	bool echo{false};
	bool broadcast{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		if (argc < 2 || argc > 4) return false;
		port = stoi(argv[1]);

		if(argc > 2){
			if (strcmp(argv[2], "-e") != 0) return false;
			echo = true;
			if (argc > 3){
				if (strcmp(argv[3], "-b") != 0) return false;
				broadcast = true;
			}
		}
		
		return port != 0;
	}
} param;

void recvThread(int sd) {
	cout << "connected\n";
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			cerr << "recv return " << res << endl;
			perror("recv");
			break;
		}
		buf[res] = '\0';

		cout << buf << endl;
		
		if (param.echo) { //if echo? send it again.
			if (param.broadcast) { //if broadcast? send to all descriptor
				set<int>::iterator it;
				for(it=sds.begin(); it!=sds.end(); it++){
					res = send(*it, buf, res, 0);
					if (res == 0 || res == -1) {
						cerr << "send return " << res << endl;
						perror("send");
						break;
					}
				}
			}
			else { //not broadcast, just echo.
				res = send(sd, buf, res, 0);
				if (res == 0 || res == -1) {
					cerr << "send return " << res << endl;
					perror("send");
					break;
				}
			}
		}
	}
	cout << "disconnected\n";
	sds.erase(sds.find(sd)); //remove socket descriptor
    close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}

	int optval = 1;
	int res = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(param.port);

	ssize_t res2 = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res2 == -1) {
		perror("bind");
		return -1;
	}

	res = listen(sd, 5);
	if (res == -1) {
		perror("listen");
		return -1;
	}

	while (true) {
		struct sockaddr_in cli_addr;
		socklen_t len = sizeof(cli_addr);
		int cli_sd = accept(sd, (struct sockaddr *)&cli_addr, &len); //connect accept.
		if (cli_sd == -1) {
			perror("accept");
			break;
		}
		sds.insert(cli_sd); //making socket descriptor set
		thread* t = new thread(recvThread, cli_sd);
		t->detach();
	}
	close(sd);
}
