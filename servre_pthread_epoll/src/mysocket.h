#pragma once
#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>  
#include <ctype.h>
void err_report(const char* str);

int Socket(int domain, int type, int protocol);
int Bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int Listen(int sockfd, int backlog);
int Accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);

int Connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);

ssize_t Write(int __fd, const void* __buf, size_t __n);
ssize_t Read(int __fd, void* __buf, size_t __nbytes);
