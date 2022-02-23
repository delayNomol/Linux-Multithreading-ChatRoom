#include "main.h"

int root_ev_fd;		//全局epoll红黑树跟
Add_list add_list;	//全局客户端地址表

/*线程创建*/
Thread_Pool* Threadpool_create(int min_thread_num, int max_thread_num, int queue_maxsize)
{
	Thread_Pool* pool = (Thread_Pool*)malloc(sizeof(Thread_Pool));
	pool->min_thread_num = min_thread_num;	//最小线程数
	pool->max_thread_num = max_thread_num;  //最大线程数
	pool->current_thread_num = min_thread_num; //创建时的基础线程
	pool->working_thread_num = 0;
	pool->need_destroy_thread_num = 0;
	pool->queue_front = 0;
	pool->queue_rear = 0;
	pool->queue_waitfor_num = 0;
	pool->queue_maxsize = queue_maxsize; //最大任务列表
	pool->shutdown = false;
	//创建工作线程id数组
	pool->worker_thread_tids = (pthread_t*)malloc(sizeof(pthread_t*) * max_thread_num);
	memset(pool->worker_thread_tids, 0, sizeof(pthread_t*) * max_thread_num);

	//创建任务队列数组
	pool->task_queue = (Thead_Tasks*)malloc(sizeof(Thead_Tasks) * queue_maxsize);
	/*初始化锁与信号量*/
	pthread_mutex_init(&pool->main_Lock, NULL);
	pthread_mutex_init(&pool->busy_thread_lock, NULL);
	pthread_cond_init(&pool->goto_work, NULL);
	pthread_cond_init(&pool->queue_not_full, NULL);

	pthread_create(&pool->admin_thread_tid, NULL, Admin_thread, pool); //创建管理线程
	for (int i = 0; i < min_thread_num; i++)
	{
		pthread_create(&pool->worker_thread_tids[i], NULL, Worker_thread, pool); //创建初始min数量的工作线程
	}

	return pool;
}
/*初始化监听*/
void Initlistenconnect(const int* root_ev_fd, unsigned short serv_port, int* lfd)
{
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(serv_port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int listenfd;
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);
	fcntl(listenfd, F_SETFL, O_NONBLOCK);
	int opt = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt)); //端口复用
	Bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	Listen(listenfd, 128);

	struct epoll_event listevent;
	memset(&listevent, 0, sizeof(listevent));
	listevent.events = EPOLLIN;
	listevent.data.fd = listenfd;
	epoll_ctl(*root_ev_fd, EPOLL_CTL_ADD, listenfd, &listevent);

	*lfd = listenfd;
	return;
}
/*任务添加*/
void Taskqueue_add(Thread_Pool* thr_pool, struct epoll_event event)
{
	printf("----Taskqueue_add调用-------\n");
	pthread_mutex_lock(&(thr_pool->main_Lock));
	if (thr_pool->queue_waitfor_num == thr_pool->queue_maxsize)  //如果等待处理的队列满了则调用信号wait阻塞
	{
		pthread_cond_wait(&(thr_pool->queue_not_full), &(thr_pool->main_Lock));
	}
	if (event.events & EPOLLIN)			//读IO任务
	{
		thr_pool->task_queue[thr_pool->queue_rear].function = Process;
		thr_pool->task_queue[thr_pool->queue_rear].fd = event.data.fd;
	}
	else if (event.events & EPOLLOUT)	//写IO任务暂不处理
	{
	}
	else {}//EPOLLERR暂不处理

	thr_pool->queue_rear = (thr_pool->queue_rear + 1) % thr_pool->queue_maxsize; //模拟循环队列
	thr_pool->queue_waitfor_num++;						//待处理的任务数++

	pthread_cond_signal(&(thr_pool->goto_work));		//唤醒线程去处理任务
	pthread_mutex_unlock(&(thr_pool->main_Lock));		//调用完线程池结构体解除线程池锁

	return;
}
/*管理线程工作函数*/
void* Admin_thread(void* pool)
{
	
	Thread_Pool* thr_pool = (Thread_Pool*)pool;
	while (!thr_pool->shutdown)
	{
		sleep(5);

		pthread_mutex_lock(&(thr_pool->main_Lock));
		int queue_waitfor_num = thr_pool->queue_waitfor_num;
		int current_thread_num = thr_pool->current_thread_num;
		pthread_mutex_unlock(&(thr_pool->main_Lock));

		pthread_mutex_lock(&(thr_pool->busy_thread_lock));
		int working_thread_num = thr_pool->working_thread_num;
		pthread_mutex_unlock(&(thr_pool->busy_thread_lock));

		/*线程不足时，创建线程*/
		if (queue_waitfor_num >= MIN_WAIT_TASK_NUM && current_thread_num < thr_pool->max_thread_num)
		{
			pthread_mutex_lock(&(thr_pool->main_Lock));
			int add = 0;

			/*一次增加 DEFAULT_THREAD 个线程*/
			for (int i = 0; i < thr_pool->max_thread_num && add < ADDSUB_THREAD_NUM && thr_pool->current_thread_num < thr_pool->max_thread_num; i++)
			{
				if (thr_pool->worker_thread_tids[i] == 0 || !Is_thread_alive(thr_pool->worker_thread_tids[i]))
				{
					pthread_create(&(thr_pool->worker_thread_tids[i]), NULL, Worker_thread, pool);
					add++;
					thr_pool->current_thread_num++;
				}
			}

			pthread_mutex_unlock(&(thr_pool->main_Lock));
			printf("---线程过少增加线程----剩余%d\n", thr_pool->current_thread_num);
		}

		/*线程过多时，减少线程*/
		if ((working_thread_num * 2) < current_thread_num && current_thread_num > thr_pool->min_thread_num)
		{

			pthread_mutex_lock(&(thr_pool->main_Lock));
			thr_pool->need_destroy_thread_num = ADDSUB_THREAD_NUM;
			pthread_mutex_unlock(&(thr_pool->main_Lock));

			for (int i = 0; i < ADDSUB_THREAD_NUM; i++)
			{
				pthread_cond_signal(&(thr_pool->goto_work));  //激活线程让其自毁
			}
			printf("---线程过多减少线程----剩余%d\n", thr_pool->current_thread_num);
		}

	}
	return NULL;
}
/*检测线程算否存在*/
int Is_thread_alive(pthread_t tid)
{
	int kill_rc = pthread_kill(tid, 0);     //发0号信号，测试线程是否存活
	if (kill_rc == ESRCH) {
		return false;
	}
	return true;
}
/*业务线程工作函数*/
void* Worker_thread(void* pool)
{
	Thread_Pool* thr_pool = (Thread_Pool*)pool;

	while (true)
	{
		pthread_mutex_lock(&(thr_pool->main_Lock));

		while (thr_pool->queue_waitfor_num == 0)	//如果队列中没有需要处理的任务则调用wait阻塞等待
		{
			printf("----lock-------%ld\n", pthread_self());
			pthread_cond_wait(&(thr_pool->goto_work), &(thr_pool->main_Lock));

			if ((thr_pool->need_destroy_thread_num > 0) && (!thr_pool->shutdown))		//需要进行自毁
			{
				thr_pool->need_destroy_thread_num--;

				if (thr_pool->current_thread_num > thr_pool->min_thread_num)	//当前线程数大于最小线程数时，进行自毁
				{
					thr_pool->current_thread_num--;
					pthread_mutex_unlock(&(thr_pool->main_Lock));
					pthread_exit(NULL);			//进行自毁
				}
			}
			printf("----waitlock-------%ld\n", pthread_self());
		}
		if (thr_pool->shutdown) 
		{
			pthread_mutex_lock(&(thr_pool->main_Lock));
			pthread_detach(pthread_self()); //线程分离
			pthread_exit(NULL);             // 线程自行结束
		}

		printf("----待处理任务数-------%d\n", thr_pool->queue_waitfor_num);
		//开始处理任务
		Thead_Tasks task;
		memset(&task, 0, sizeof(task));
		task.function = thr_pool->task_queue[thr_pool->queue_front].function;	//从任务列表取出任务
		task.fd = thr_pool->task_queue[thr_pool->queue_front].fd;
		thr_pool->queue_front = (thr_pool->queue_front + 1) % thr_pool->queue_maxsize;
		thr_pool->queue_waitfor_num--;
		pthread_cond_signal(&(thr_pool->queue_not_full));	//通知主线程可添加线程
		pthread_mutex_unlock(&(thr_pool->main_Lock));		//调用完线程池结构体解除线程池锁

		pthread_mutex_lock(&(thr_pool->busy_thread_lock));	//添加working_thread_num资源锁
		thr_pool->working_thread_num++;
		pthread_mutex_unlock(&(thr_pool->busy_thread_lock));

		task.function(task.fd);	//执行任务处理函数

		pthread_mutex_lock(&(thr_pool->busy_thread_lock));
		thr_pool->working_thread_num--;
		pthread_mutex_unlock(&(thr_pool->busy_thread_lock));
	}
	pthread_exit(NULL);
}
/*读IO业务函数*/
void* Process(int fd) //业务处理
{
	printf("-----业务处理------%ld\n", pthread_self());
	char buf[BUFSIZ] = { 0 };
	int connectfd = fd;
	Client_Add add;
	char client_ip_str[64] = { 0 };
	Client_addlist_find(connectfd, &add);		//获取当前连接地址
	int ret = Read(connectfd, buf, sizeof(buf));
	if (ret == 0) //读到socket为空，对方已断开连接
	{

		printf("%ld", pthread_self());
		printf("客户端 ip:%s port:%d 断开连接\n",
			inet_ntop(AF_INET, &add.client_addr.sin_addr.s_addr, client_ip_str, sizeof(client_ip_str)),
			ntohs(add.client_addr.sin_port));

		close(connectfd);	//关闭连接
		Client_addlist_del(connectfd); //删除在地址表中的地址
		struct epoll_event tep_event = { 0, {0} };
		epoll_ctl(root_ev_fd, EPOLL_CTL_DEL, connectfd, &tep_event);  //将该socket的监听事件从epoll红黑树上摘下
		return NULL;
	}
	write(STDOUT_FILENO, buf, ret); //输出在服务器屏幕上

	for (int i = 0; i < add_list.list_current; i++) //将buf的信息分发给所有客户端
	{
		char idbuf[64] = { 0 };

		sprintf(idbuf, "客户端 ip:%s port:%d 说：\n",
			inet_ntop(AF_INET, &add.client_addr.sin_addr.s_addr, client_ip_str, sizeof(client_ip_str)),
			ntohs(add.client_addr.sin_port));

		Write(add_list.list[i].fd, idbuf, strlen(idbuf));
		Write(add_list.list[i].fd, buf, strlen(buf));
	}
	//任务处理完成，重新添加到epoll红黑树上监听
	struct epoll_event tep_event;
	memset(&tep_event, 0, sizeof(tep_event));
	tep_event.events = EPOLLIN;
	tep_event.data.fd = connectfd;
	epoll_ctl(root_ev_fd, EPOLL_CTL_ADD, connectfd, &tep_event); //重新添加到epoll红黑树上监听

	return NULL;
}
/*客户端地址表，添加*/
void Client_addlist_add(int fd, sockaddr_in tep_client_addr, int tep_client_len)
{
	printf("-----client_addlist_add调用---\n");
	if (add_list.list_current < add_list.list_maxsize)
	{

		add_list.list[add_list.list_current].fd = fd;
		add_list.list[add_list.list_current].client_addr = tep_client_addr;
		add_list.list[add_list.list_current].client_len = tep_client_len;
		add_list.list_current++;

	}

	return;
}
/*客户端地址表，删除*/
void Client_addlist_del(int fd)
{
	if (add_list.list_current > 0)
	{
		for (int i = 0; i < add_list.list_current; i++)
		{
			if (add_list.list[i].fd == fd)
			{
				add_list.list[i].fd = add_list.list[add_list.list_current - 1].fd;
				add_list.list[i].client_addr = add_list.list[add_list.list_current - 1].client_addr;
				add_list.list[i].client_len = add_list.list[add_list.list_current - 1].client_len;
				add_list.list_current--;
			}
		}
	}
	return;
}
/*客户端地址表，查找*/
int Client_addlist_find(int fd, Client_Add* add)
{
	for (int i = 0; i < add_list.list_current; i++)
	{
		if (add_list.list[i].fd == fd)
		{
			*add = add_list.list[i];
			return 0;
		}
	}
	return -1;	//未找到
}
/*删除线程池*/
int Threadpool_del(Thread_Pool* pool)
{
	int i;
	if (pool == NULL) {
		return -1;
	}
	pool->shutdown = true;

	/*回收管理线程*/
	pthread_join(pool->admin_thread_tid, NULL);

	for (i = 0; i < pool->current_thread_num; i++) {
		/*激活所有空闲进程进行自毁*/
		pthread_cond_broadcast(&(pool->goto_work));
	}
	for (i = 0; i < pool->current_thread_num; i++) {
		pthread_join(pool->worker_thread_tids[i], NULL);
	}

	/*释放内存*/
	if (pool->task_queue) 
	{
		free(pool->task_queue);		//释放任务队列数组内存空间
	}
	if (pool->worker_thread_tids)	//释放工作线程标识id数组内存空间
	{
		free(pool->worker_thread_tids);
		pthread_mutex_lock(&(pool->main_Lock));
		pthread_mutex_destroy(&(pool->main_Lock));			//销毁线程池结构体主锁
		pthread_mutex_lock(&(pool->busy_thread_lock));
		pthread_mutex_destroy(&(pool->busy_thread_lock));	//working_thread_num资源锁
		pthread_cond_destroy(&(pool->goto_work));			//销毁激活工作线程信号
		pthread_cond_destroy(&(pool->queue_not_full));		//销毁任务队列不为空信号
	}
	free(pool);		//释放线程池内存空间
	pool = NULL;	//清除空指针

	return 0;
}
/*程序入口函数*/
int main(int argc, char* argv[])
{

	Thread_Pool* thr_pool = Threadpool_create(4, 100, 100);
	root_ev_fd = epoll_create(MAX_EVENTS);
	int listenfd, connectfd;
	Initlistenconnect(&root_ev_fd, SERV_PORT, &listenfd);
	struct epoll_event event_queue[MAX_EVENTS];
	add_list.list_maxsize = MAX_EVENTS;
	add_list.list_current = 0;

	struct sockaddr_in tep_client_addr;
	memset(&tep_client_addr, 0, sizeof(tep_client_addr));
	socklen_t tep_client_len;
	struct epoll_event tep_event;
	memset(&tep_event, 0, sizeof(tep_event));

	while (true)
	{
		int ready_num = epoll_wait(root_ev_fd, event_queue, MAX_EVENTS, 1000); //监听epoll红黑树
		for (int i = 0; i < ready_num; i++)
		{
			if (event_queue[i].events & EPOLLIN && event_queue[i].data.fd == listenfd) //有客户端连接请求
			{
				tep_client_len = sizeof(tep_client_addr);

				connectfd = Accept(listenfd, (struct sockaddr*)&tep_client_addr, &tep_client_len); //与客户端建立连接
				printf("----Accept-------fd：%d\n", connectfd);
				Client_addlist_add(connectfd, tep_client_addr, tep_client_len);	//将地址添加到全局地址表

				tep_event.events = EPOLLIN;
				tep_event.data.fd = connectfd;
				epoll_ctl(root_ev_fd, EPOLL_CTL_ADD, connectfd, &tep_event); //将与客户端连接的socket添加到epoll红黑树上监听
				printf("----epoll_ctl-------\n");

			}
			else //不是连接请求，则是业务处理事件
			{
				Taskqueue_add(thr_pool, event_queue[i]);   //将请求事件添加到任务列表
				struct epoll_event epv = { 0, {0} };
				epoll_ctl(root_ev_fd, EPOLL_CTL_DEL, event_queue[i].data.fd, &epv);     //将对应fd从红黑树摘除监听

			}

		}


	}

	Threadpool_del(thr_pool); //删除线程池
	return 0;
}