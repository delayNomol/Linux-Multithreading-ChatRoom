#include "mysocket.h"

/*ÅÐ¶Ïº¯Êý´íÎó·µ»ØÖµº¯Êý¿â*/

void err_report(const char* str)
{
	perror(str);
	exit(1);
}

int Socket(int domain, int type, int protocol)
{
	int n = socket(domain, type, protocol);
	if (n == -1)
	{
		err_report("socket error");
	}
	return n;
}

int Bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
	int n = bind(sockfd, addr, addrlen);
	if (n == -1)
	{
		err_report("bind error");
	}
	return n;
}

int Listen(int sockfd, int backlog)
{
	int n = listen(sockfd, backlog);
	if (n == -1) {
		err_report("listen error");
	}
	return n;
}

int Accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
	int n = accept(sockfd, addr, addrlen);
	if (n == -1) {
		err_report("accept error");
	}
	return n;
}


int Connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
	int n = connect(sockfd, addr, addrlen);
	if (n == -1) {
		err_report("connect error");
	}
	return n;
}
ssize_t Write(int __fd, const void* __buf, size_t __n)
{
	ssize_t ret;

	while ((ret = write(__fd, __buf, __n)) == -1)
	{
		if (errno == EAGAIN)
		{
			continue;
		}
		else if (errno == EINTR)
		{
			continue;
		}
		else
		{
			err_report("Write error");
		}
	}
	return ret;
}

ssize_t Read(int __fd, void* __buf, size_t __nbytes)
{

	ssize_t ret;

	while ((ret = read(__fd, __buf, __nbytes)) == -1)
	{
		if (errno == EAGAIN)
		{
			continue;
		}
		else if (errno == EINTR)
		{
			continue;
		}
		else
		{
			err_report("Read error");
		}
	}
	return ret;
}
