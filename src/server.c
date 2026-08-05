#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT "8888"
#define BACKLOG 5 // maximum number of pending connections in the queue
#define DATA_BUF_SIZE 500

int main(void) {

	// sockaddr_in is identical to sockaddr.
	// sockaddr_in (in stands for internet)
	// was created for convinience
	// in order to reference the elements of the socket address.

	// Given this is the server application we want to initialize a stream
	// socket, bind to 8888 port and listen to incomming connections from
	// any ip.

	struct addrinfo hints, *result, *p;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // dont care if we use ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags =
	    AI_PASSIVE; // I intend to listen to everyone (all adresses and
			// interfaces), give me the wildcard adress (0.0.0.0)

	char addr_str[INET6_ADDRSTRLEN];
	void *addr;
	char *ipver;
	struct sockaddr_in *ipv4;
	struct sockaddr_in6 *ipv6;

	if (getaddrinfo(NULL, PORT, &hints, &result) != 0) {
		fprintf(stderr,
			"unable to fill out information for server socket\n");
		exit(EXIT_FAILURE);
	}

	int s;
	// go through each element in the linked list and show IP's
	for (p = result; p != NULL; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (s == -1) {
			fprintf(stderr, "Could not create a socket\n");
			continue;
		}

		// we need to bind the socket to the port we are going to use so
		// that the kernel knows where (to what process it should route
		// the network packets coming in).
		if (bind(s, p->ai_addr, p->ai_addrlen) == -1) {
			fprintf(stderr, "Could not bind the socket\n");
			continue;
		}
		break;
	}
	freeaddrinfo(result);
	printf("listening...\n");
	if (listen(s, BACKLOG) == -1) {
		fprintf(stderr, "Could not listen\n");
		exit(EXIT_FAILURE);
	}

	socklen_t addr_size = sizeof(struct sockaddr_storage);
	struct sockaddr_storage client_addr;
	int receive_s;
	printf("waiting for client...\n");
	receive_s = accept(s, (struct sockaddr *)&client_addr, &addr_size);
	if (receive_s == -1) {
		fprintf(stderr, "Could not accept\n");
		exit(EXIT_FAILURE);
	}
	char data[DATA_BUF_SIZE];
	recv(receive_s, data, DATA_BUF_SIZE, 0);
	printf("%s\n", data);
	close(s);
	return EXIT_SUCCESS;
}
