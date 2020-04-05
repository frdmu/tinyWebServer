#ifndef RIO_H_INCLUDED
#define RIO_H_INCLUDED
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#define RIO_BUFSIZE 8192

/* Internal buf*/
typedef struct {
	int rio_fd;
	int rio_cnt;
	char *rio_bufptr;
	char rio_buf[RIO_BUFSIZE];
} rio_t;
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<Unbuffer input and output function>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/* unbuf version read */
ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
	size_t nleft = n;
	ssize_t nread;
	char *buf = usrbuf;

	while (nleft > 0) {
		if ((nread = read(fd, buf, nleft)) < 0) {
			if (errno == EINTR) {// Interrupted error
				nread = 0;
			} else {
				return -1;
			}		
		} else if (nread == 0) {
			break;
		}
		nleft -= nread;
		buf += nread;	
	}

	return (n-nleft);
}
/* unbuf version writen */
ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
	size_t nleft = n;
	ssize_t nwrite;
	char *buf = usrbuf;

	while (nleft > 0) {
		if ((nwrite = write(fd, buf, nleft)) <= 0) {
			if (errno == EINTR)
				nwrite = 0;
			else
				return -1;
		} 	
		nleft -= nwrite;
		buf += nwrite;
	}

	return n;
}


/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<Buffer input function>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/* Initial struct rio_t*/
void rio_readinitb(rio_t *rp, int fd)
{
	rp->rio_fd = fd;
	rp->rio_cnt = 0;
	rp->rio_bufptr = rp->rio_buf;
}
/* buf version read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
	int cnt;

	// Check the internal buf empty or not	
	// Refill the internal buf if it's empty	
	while (rp->rio_cnt <= 0) {
		rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
		if (rp->rio_cnt < 0) {
			if (errno != EINTR) // Interruptd by sig handler return 
				return -1; 
		} else if (rp->rio_cnt == 0) {
			return 0; // EOF
		} else {
			rp->rio_bufptr = rp->rio_buf;
		}
	}

	// Copy min(n, rp->rio_cnt) bytes from internal buf to user buf 
	cnt = n;
	if (rp->rio_cnt < n)
		cnt = rp->rio_cnt;
	memcpy(usrbuf, rp->rio_bufptr, cnt);
	rp->rio_bufptr += cnt;
	rp->rio_cnt -= cnt;
	
	return cnt;	// Return the number of bytes it has read from internal buf into usrbuf
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
	int n, rc;
	char c, *bufp = usrbuf;

	for (n = 1; n < maxlen; n++) {
		if ((rc = rio_read(rp, &c, 1)) == 1) {
			*bufp++ = c;
			if (c == '\n') {
				n++;
				break;
			}
		} else if (rc == 0) {
			if (n == 1) 
				return 0;
			else
				break;
		} else {
			return -1;
		}
	}

	*bufp = 0;
	return (n-1);	
}

ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
	size_t nleft = n;
	ssize_t nread;
	char *bufp = usrbuf;
	
	while (nleft > 0) {
		if ((nread = rio_read(rp, bufp, nleft)) < 0)
			return -1;
		else if (nread == 0)
			break;	

		nleft -= nread;
		bufp += nread;
	}	

	return (n-nleft);
}
#endif
