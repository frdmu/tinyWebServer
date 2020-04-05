#ifndef WEB_H_INCLUDED
#define WEB_H_INCLUDED

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
// establish a socket in client
int open_clientfd(char *hostname, char *port)
{
	int clientfd;
	struct addrinfo hints, *listp, *p;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_flags |= AI_ADDRCONFIG;
	getaddrinfo(hostname, port, &hints, &listp);

	for (p = listp; p; p = p->ai_next) {
		if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;	
		
		if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) 
			break;
		close(clientfd);// connect failed, try another
	}

	freeaddrinfo(listp);
	if (!p) // all failed
		return -1;
	else
		return clientfd;
}

// establish a socket in server
int open_listenfd(char *port)
{
	struct addrinfo hints, *listp, *p;
	int listenfd, optval=1;

	/* Get a list of potential server addresses */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
	getaddrinfo(NULL, port, &hints, &listp);

	/* Walk the list for one that we can bind to */
	for (p = listp; p; p = p->ai_next) {
		/* Create a socket descriptor */
		if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
			continue;// Failed
		}

		/* Permit address reuse */
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

		/* Bind the descriptor with the socket*/
		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
			break;// Success
		}
		
		close(listenfd);
	}

	/* Free list: listp*/
	freeaddrinfo(listp);
	if (!p) /* No address worked */
		return -1;

	/* Make it a listening socket ready to accept connection requests */
	if (listen(listenfd, 1024) < 0) {
		close(listenfd);
		return -1;
	}	

	return listenfd;
}

#endif 
