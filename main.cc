#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/epoll.h>

using namespace std;

#define EPOLL_SIZE 1000
#define BUF_SIZE 10000000

//fuck!!!!
struct sockaddr_in serv_addr;

//returns client socket in case of success
int client_initialization (string serv_ip_addr, int serv_port) {

	//create client socket
	int clnt_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(clnt_sock == -1)
		return -1;
	cout << "Socket Creation OK" << endl;

	//set up destined server addr
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(serv_ip_addr.c_str());
	serv_addr.sin_port = htons(serv_port);

	//try connection to the server
	if(connect(clnt_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
		return -1;
	cout << "connection OK" << endl;
	return clnt_sock;
}

int client_duplex(int clnt_sock) {

	while(true) {
		string user_input;
		cin >> user_input;
		write(clnt_sock, user_input.c_str(), user_input.size());
	}
}

//returns server socket in case of success
int server_initialization (int port) {

	//create server socket
	int serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		return -1;

	//fill up server address
	memset(&serv_addr, 0, sizeof(serv_addr));
	struct ifaddrs * addrs;
	if(getifaddrs(&addrs) == -1)
		return -1;
	struct ifaddrs * tmp = addrs;
	//find server address from interface list of the local PC
	while (tmp) {
		if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET &&
				!(tmp->ifa_flags & IFF_LOOPBACK) && tmp->ifa_flags & IFF_RUNNING) {
			struct sockaddr_in * tmp_addr = (struct sockaddr_in *)tmp->ifa_addr;
			serv_addr.sin_addr.s_addr = tmp_addr->sin_addr.s_addr;
			break;
		}
		tmp = tmp->ifa_next;
	}
	freeifaddrs(addrs);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	//bind the socket to the server address
	if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
		return -1;

	return serv_sock;
}

int server_multiplexing (int serv_sock) {

	int epfd, event_cnt;
	struct epoll_event *ep_events;
	struct epoll_event event;

	epfd = epoll_create(EPOLL_SIZE); // size 1000 is just a dummy variable
	// linux kernel 2.6.8 and above automatically adjusts the size and capacity of file descriptor amount
	//However, EPOLL_SIZE is useful for setting up ep_events which receive event from the kernel
	ep_events = (struct epoll_event*)malloc(sizeof(struct epoll_event)*EPOLL_SIZE);

	event.events = EPOLLIN;
	event.data.fd = serv_sock;
	epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

	while(true) {
		
		//cout << "Epoll waiting...." << endl;
		event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
		if(event_cnt == -1) {
			cout << "Epoll wait fails, stop multiplexing" << endl;
			close(epfd);
			return -1;
		}

		for(int i=0; i < event_cnt; i++) {
			if(ep_events[i].data.fd==serv_sock) {
				//new connection has arrived!	
				struct sockaddr_in clnt_addr;
				socklen_t clnt_addr_size = sizeof(clnt_addr);
				int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
				if(clnt_sock == -1)
					return -1;
				cout << "Accept done" << endl;
				event.events=EPOLLIN;
				event.data.fd=clnt_sock;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
			}
			else {
				char buf [BUF_SIZE];
				cout << buf << endl;
				int str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);
				if(str_len == 0) {
					//close_request
					epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
					close(ep_events[i].data.fd);
					cout << "Client closed" << endl;
				}
				else {
					cout << "Client FD : " << ep_events[i].data.fd << " : " << buf << endl;
				}
			}
		}
	}
	close(epfd);

}


int main () {

	while (true) {

		char use;
		int port;
		// check client or server
		cout << "Use as a client or a server? (c / s) : " << endl;
		cin >> use;

		// if server, go to server function
		if (use == 's'){
			cout << "Give us your preferred port number : " << endl;
			cin >> port;
			int serv_sock = server_initialization(port);
			if(serv_sock == -1) {
				cout << "Server socket creation is failed, lets do it again" << endl;
				continue;
			}
			cout << "Server Socket Well created" << endl;
			char buffer[20];
			inet_ntop(AF_INET, &(serv_addr.sin_addr), buffer, 20);
			cout << "IP address : " << buffer << endl;
			cout << "Port number : " << port << endl;

			//set the socket to be passive (listening) socket
			if(listen(serv_sock, 5) == -1)
				return -1;
	
			cout << "MULTIPLEXING" << endl;
			//time to operate I/O multiplexing
			server_multiplexing(serv_sock);

			break;
		}

		// if client, go to client function
		else if (use == 'c') {

			string serv_ip_addr;
			cout << "Server IP address : " << endl;
			cin >> serv_ip_addr;
			cout << "Server port number : " << endl;
			cin >> port;
			int clnt_sock = client_initialization (serv_ip_addr, port);
			if(clnt_sock == -1) {
				cout << "Client socket creation is failed, lets do it again" << endl;
				continue;
			}
			cout << "Connection to the server accomplished!" << endl;
			client_duplex(clnt_sock);
			break;
		}
		else
			cout << "Wrong Input, do it again" << endl;
	}
return 0;

}


