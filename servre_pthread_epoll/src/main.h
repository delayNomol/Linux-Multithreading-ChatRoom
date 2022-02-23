#pragma once
#include <cstdio>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>  
#include <sys/socket.h>  
#include <sys/epoll.h>  
#include <arpa/inet.h>  
#include "mysocket.h"
#define SERV_PORT 10086
#define MAX_EVENTS 1024			 //最大连接数
#define MIN_WAIT_TASK_NUM 10	//最任务等待数
#define ADDSUB_THREAD_NUM 10	//每次添加/减少线程的个数

/*任务结构*/
typedef struct
{
	void* (*function)(int);		//工作线程回调任务函数
	int fd;						//任务函数参数
}Thead_Tasks;

/*自定义地址结构*/
typedef struct
{
	int fd;
	struct sockaddr_in client_addr;		//客户端连接地址
	socklen_t client_len;				//客户端地址长度
}Client_Add;

/*自定义地址结构表*/
typedef struct 
{
	Client_Add list[MAX_EVENTS+1];
	int list_current;
	int list_maxsize;
}Add_list;

/*线程池结构体*/
struct Thread_Pool
{
	pthread_mutex_t main_Lock;          //线程池结构体主锁
	pthread_mutex_t busy_thread_lock;	//working_thread_num资源锁

	pthread_cond_t goto_work;			//激活工作线程信号
	pthread_cond_t queue_not_full;		//任务队列不为空信号

	pthread_t* worker_thread_tids;		//工作线程标识id数组
	pthread_t admin_thread_tid;			//管理线程标识id
	int min_thread_num;					//最小线程并发数
	int max_thread_num;					//最大线程并发数
	int current_thread_num;				//现有线程数
	int working_thread_num;				//正在处理任务的线程数
	int need_destroy_thread_num;		//需要销毁的线程数

	Thead_Tasks* task_queue;			//任务队列数组
	int queue_front;					//任务队列头标志
	int queue_rear;						//任务队列尾标志
	int queue_waitfor_num;				//等待处理的任务数
	int queue_maxsize;					//任务队列最大数

	int shutdown;		 //线程池状态1表示使用，0表示关闭并销毁
};

Thread_Pool* Threadpool_create(int min_thread_num, int max_thread_num, int queue_maxsize);
void Initlistenconnect(const int* root_ev_fd, unsigned short serv_port, int* lfd);
void Taskqueue_add(Thread_Pool* thr_pool, struct epoll_event event);
void* Admin_thread(void* pool);
void* Worker_thread(void* pool);
void* Process(int fd);
void Client_addlist_add(int fd, sockaddr_in tep_client_addr, int tep_client_len);
void Client_addlist_del(int fd);
int Client_addlist_find(int fd, Client_Add* add);
int Is_thread_alive(pthread_t tid);
int Threadpool_del(Thread_Pool* pool);
