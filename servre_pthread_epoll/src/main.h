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
#define MAX_EVENTS 1024			 //���������
#define MIN_WAIT_TASK_NUM 10	//������ȴ���
#define ADDSUB_THREAD_NUM 10	//ÿ�����/�����̵߳ĸ���

/*����ṹ*/
typedef struct
{
	void* (*function)(int);		//�����̻߳ص�������
	int fd;						//����������
}Thead_Tasks;

/*�Զ����ַ�ṹ*/
typedef struct
{
	int fd;
	struct sockaddr_in client_addr;		//�ͻ������ӵ�ַ
	socklen_t client_len;				//�ͻ��˵�ַ����
}Client_Add;

/*�Զ����ַ�ṹ��*/
typedef struct 
{
	Client_Add list[MAX_EVENTS+1];
	int list_current;
	int list_maxsize;
}Add_list;

/*�̳߳ؽṹ��*/
struct Thread_Pool
{
	pthread_mutex_t main_Lock;          //�̳߳ؽṹ������
	pthread_mutex_t busy_thread_lock;	//working_thread_num��Դ��

	pthread_cond_t goto_work;			//������߳��ź�
	pthread_cond_t queue_not_full;		//������в�Ϊ���ź�

	pthread_t* worker_thread_tids;		//�����̱߳�ʶid����
	pthread_t admin_thread_tid;			//�����̱߳�ʶid
	int min_thread_num;					//��С�̲߳�����
	int max_thread_num;					//����̲߳�����
	int current_thread_num;				//�����߳���
	int working_thread_num;				//���ڴ���������߳���
	int need_destroy_thread_num;		//��Ҫ���ٵ��߳���

	Thead_Tasks* task_queue;			//�����������
	int queue_front;					//�������ͷ��־
	int queue_rear;						//�������β��־
	int queue_waitfor_num;				//�ȴ������������
	int queue_maxsize;					//������������

	int shutdown;		 //�̳߳�״̬1��ʾʹ�ã�0��ʾ�رղ�����
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
