#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define PORT "8888"

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
	hints.ai_flags = AI_PASSIVE; // I intend to listen to everyone, give me
				     // the wildcard adress (0.0.0.0)

	char addr_str[INET6_ADDRSTRLEN];
	void *addr;
	char *ipver;
	struct sockaddr_in *ipv4;
	struct sockaddr_in6 *ipv6;

	if (getaddrinfo(NULL, PORT, &hints, &result) != 0)
		fprintf(stderr,
			"unable to fill out information for server socket\n");
	else
		fprintf(stdout, "filled out info for server socket\n");

	// go through each element in the linked list and show IP's
	for (p = result; p != NULL; p = p->ai_next) {
		if (p->ai_family == AF_INET) {
			ipv4 = (struct sockaddr_in *)p->ai_addr;
			addr = &(ipv4->sin_addr);
			ipver = "IPv4";
		} else {
			ipv6 = (struct sockaddr_in6 *)p->ai_addr;
			addr = &(ipv6->sin6_addr);
			ipver = "IPv6";
		}
		inet_ntop(p->ai_family, addr, addr_str, sizeof addr_str);
		printf("adress found:\nIP version: %s\nadress: %s\n", ipver,
		       addr_str);
	}

	freeaddrinfo(result);
	return EXIT_SUCCESS;
}
