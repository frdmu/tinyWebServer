/*
 *	tiny_web_server.c - A sample, iterative HTTP/1.0 web server that uses the
 *	Get method to serve static and dynamic content.
 */
#include "web.h"
#include "rio.h"
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/wait.h>
#define MAXLINE 50
#define MAXBUF 2048
extern char **environ; // Must decalare use extern 

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	char buf[MAXLINE], body[MAXBUF];

	/* Build the HTTP response body */
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	rio_writen(fd, buf, sizeof(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	rio_writen(fd, buf, sizeof(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	rio_writen(fd, buf, sizeof(buf));	
	rio_writen(fd, body, sizeof(body));
}
void read_requesthdrs(rio_t *rp)
{
	char buf[MAXLINE];

	rio_readlineb(rp, buf, sizeof(buf));
	while (strcmp(buf, "\r\n")) {
		rio_readlineb(rp, buf, sizeof(buf));
		printf("%s", buf);
	}
}
int parse_uri(char *uri, char *filename, char *cgiargs)
{
	char *ptr;

	if (!strstr(uri, "cgi-bin")) { /* Static content such as: "/", "/index.html" */
		strcpy(cgiargs, ""); // cgiargs is null
		strcpy(filename, "./resource");
		strcat(filename, uri);
		if (uri[strlen(uri) - 1] == '/')
			strcat(filename, "home.html");
			return 1;
	} else { /* Dynamic content such as: "/add?100&30"*/
		ptr = index(uri, '?');
		if (ptr) {
			strcpy(cgiargs, ptr+1);
			*ptr = '\0';
		} else {
			strcpy(cgiargs, ""); // No argument value
		}	
		strcpy(filename, "./resource");
		strcat(filename, uri);
		return 0;	
	}
}

void get_filetype(char *filename, char *filetype)
{
	if (strstr(filename, ".html")) 
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".gif")) 
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".png")) 
		strcpy(filetype, "image/png");
	else if (strstr(filename, ".jpg")) 
		strcpy(filetype, "image/jpg");
	else	
		strcpy(filetype, "text/plain");
}
/* Send filename to client */
void serve_static(int fd, char *filename, int filesize)
{
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];
	
	/* Send response headers to client */
	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);	
	sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	rio_writen(fd, buf, strlen(buf));
	printf("Response headers:\n");
	printf("%s", buf);

	/* Send response body to client */
	srcfd = open(filename, O_RDONLY, 0);
	srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);// Use mmap(), operate the memory "srcp" directly 
															// and don't need use read() and write().
	close(srcfd);
	rio_writen(fd, srcp, filesize);
	munmap(srcp, filesize);	
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf[MAXLINE], *emptylist[] = {NULL};

	/* Return first part of HTTP response */
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Tiny Web Server\r\n");
	rio_writen(fd, buf, strlen(buf));

	if (fork() == 0) { // Child
		/* Real server would set all CGI vars here */
		setenv("QUERY_STRING", cgiargs, 1);
		dup2(fd, STDOUT_FILENO); // Redirect stdout to client
		execve(filename, emptylist, environ);
	}
	wait(NULL);
}
void doit(int fd)
{
	int is_static;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio; // Buf corresponding to fd

	/* Read request line and headers */
	rio_readinitb(&rio, fd );
	rio_readlineb(&rio, buf, MAXLINE);
	printf("Request line:\n");
	printf("%s", buf);
	sscanf(buf, "%s %s %s", method, uri, version);
	if (strcasecmp(method, "GET")) {
		clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
		return;
	}
	printf("Request headers:\n");
	read_requesthdrs(&rio);

	/* Parse uri from GET request */
	is_static = parse_uri(uri, filename, cgiargs);	
	if (stat(filename, &sbuf) < 0) { // Get state of filename
		clienterror(fd, filename, "404", "Not found", "Tiny could not find this file");
		return;
	} 

	if (is_static) { /* Serve static content */
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
			return;
		}
		serve_static(fd, filename, sbuf.st_size);
	} else { /* Serve dynamic content */
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
			return;
		}
		serve_dynamic(fd, filename, cgiargs);
	}
	
}
int main(int argc, char **argv)
{
	int listenfd, connfd;
	char hostname[MAXLINE], port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;

	/* Check command-line args */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	listenfd = open_listenfd(argv[1]);
	while (1) {
		clientlen = sizeof(clientaddr);	
		connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
		getnameinfo((struct sockaddr*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		printf("Accept connection from (%s %s)\n", hostname, port);
		doit(connfd); // Deal with one transcation, i.e one GET method request.
		close(connfd);
	}

}
