#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>  
#include <ctype.h>
#include <pthread.h>
#define SERV_PORT 10086

typedef struct
{
	int fd;
}Fd;

void err_report(const char* str)
{
	perror(str);
	exit(1);
}
void* read_thread(void* arg)
{
	Fd* p_fd = (Fd*)arg;
	while (true)
	{
		char readbuf[BUFSIZ]={0};
		int ret = read(p_fd->fd, readbuf, sizeof(readbuf));
		write(STDOUT_FILENO, readbuf, ret);
	}
	return NULL;
}
Fd g_fd;
int main(int argc, char* argv[])
{

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
    int dst;
	inet_pton(AF_INET, "192.168.233.123", (void *)&dst);
	serv_addr.sin_addr.s_addr = dst;

	int fd, ret = 0;
	fd = socket(AF_INET, SOCK_STREAM, 0);             //type：SOCK_STREAM：tcp       SOCK_DGRAM：udp
	if (fd == -1) {
		err_report("socket error");
	}

	ret = connect(fd,(struct sockaddr*)& serv_addr,sizeof(serv_addr));
	if (ret == -1) {
		err_report("connect error");
	}

	char serv_ip_str[64] = { 0 };
	printf("connect succeed！server ip:%s port:%d\n",
		inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, serv_ip_str, sizeof(serv_ip_str)),
		ntohs(serv_addr.sin_port));

	
	g_fd.fd = fd;
	pthread_t tid;
	pthread_create(&tid, NULL, read_thread,(void*)&g_fd);
	

	while (true)
	{
		char writebuf[BUFSIZ]={0};
		read(STDIN_FILENO, writebuf, sizeof(writebuf));
		write(fd, writebuf, strlen(writebuf));
	}

	close(fd);
	pthread_join(tid, NULL);
	return 0;
}
