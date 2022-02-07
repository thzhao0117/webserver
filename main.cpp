#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<assert.h>
#include<sys/epoll.h>

#include"locker.h"
#include"threadpool.h"
#include"http_con.h"

#include"lst_timer.h"
#include"log.h"

#include<iostream>

#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000

#define TIMESLOT 5//最小超时时间
#define SYNLOG  //同步写日志

//设置定时器相关参数
static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;


//三个函数定义在http_conn.cpp中，改变连接属性
extern int addfd(int apollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);
extern int setnonblocking(int fd);


//设置信号函数 
void addsig(int sig, void(handler)(int), bool restart = true)
{
	//创建sigaction结构体变量
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));

	//信号处理函数中仅仅发送信号值，不做对应逻辑处理
	sa.sa_handler = handler;
	if (restart)
	{
		sa.sa_flags |= SA_RESTART;//SA_RESTART总结来说就是让中断的系统调用重新执行
	}
	//将所有信号添加到信号集中
	sigfillset(&sa.sa_mask);//用来将参数set信号集初始化，然后把所有的信号加入到此信号集里。
	//执行sigaction函数
	assert(sigaction(sig, &sa, NULL) != -1);//sigaction函数的功能是检查或修改与指定信号相关联的处理动作

}

void show_error(int connfd, const char* info)
{
	printf("%s", info);
	send(connfd, info, strlen(info), 0);
	close(connfd);
}

//信号处理函数
void sig_handler(int sig)
{
	   //为保证函数的可重入性，保留原来的errno
		    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
		    int save_errno = errno;
	    int msg = sig;
	
		    //将信号值从管道写端写入，传输字符类型，而非整型
		    send(pipefd[1], (char*)&msg, 1, 0);
	
		    //将原来的errno赋值为当前的errno
		    errno = save_errno;
	}


//定时器回调函数
void cb_func(client_data* user_data)
{
	//删除非活动连接在socket上的注册事件
	epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
	assert(user_data);

	//关闭文件描述符
	close(user_data->sockfd);

	//减少连接数
	http_con::m_user_count--;

	LOG_INFO("close fd %d", user_data->sockfd);
	Log::get_instance()->flush();
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void timer_handler()
{
	timer_lst.tick();

    alarm(TIMESLOT);
	/*它的主要功能是设置信号传送闹钟。其主要功能用来设置信号SIGALRM在经过seconds指定的秒数后传送给目前的进程，
	如果在定时未完成的时间内再次调用了alarm函数，则后一次定时器设置将覆盖前面的设置，当seconds设置为0时，
	定时器将被取消。它返回上次定时器剩余时间，如果是第一次设置则返回0。*/
}


int main(int argc, char* argv[])
{


#ifdef SYNLOG
	Log::get_instance()->init("ServerLog", 2000, 800000, 0); //同步日志模型
#endif


	if (argc <= 2)
	{
		//char *basename(char *path);

		//功能： 截取path中的去目录部分的最后的文件或路径名。

		printf("usage:%s ip_address port_number\n", basename(argv[0]));
		return 1;
		
	}

	//ip,port靠开始的时候给函数的第一，第二个参数传入。第一个参数argv[0]是程序名，由系统自动传入
	const char* ip = argv[1];
	int port = atoi(argv[2]);//atoi,讲一个字符数转化为整数

	//忽略SIGPIPE信号
	//因为如果socket的对方已经关闭连接，而我方依然在写入的情况下，会导致进程退出，因此要忽略该信号
	addsig(SIGPIPE, SIG_IGN);


	
	//创建线程池
	threadpool<http_con>* pool = NULL;
	try
	{
		pool = new threadpool<http_con>;//将可能发生错误的语句放在try保护段，出了异常就用catch接收并处理，无catch不执行

	}
	catch (...)
	{
		return 1;
	}


	/*预先为每一个可能的客户连接分配一个http_con对象*/
	http_con* users = new http_con[MAX_FD];
	//ssert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行。
	assert(users);
	int user_count = 0;

	int listenfd = socket(PF_INET, SOCK_STREAM, 0);//pf_inet,用于iPV4。sock_stream,服务类型是流服务
	assert(listenfd >= 0);
	struct linger temp = { 1,0 };
	//sol_socket,在套接字级别上设置。SO_LINGER如果有数据待发送，就延迟关闭
	//果选择此选项，close或 shutdown将等到所有套接字里排队的消息成功发送或到达延迟时间后才会返回。否则，调用将立即返回。
	////SO_LINGER若有数据待发送，延迟关闭
	setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &temp, sizeof(temp));//设定close()关闭socket的行为，temp=1,0代表close立即返回，丢弃tcp缓冲里面没有发送完成的数据

	int ret = 0;
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;//ipv4协议族
	//address.sin_addr.s_addr = htonl(INADDR_ANY);
	printf("this is address_ip_location\n");
	//printf("ip is: %s\n", *ip);
	inet_pton(AF_INET, ip, &address.sin_addr);//将ip转化为网络字节整数表示的ip地址，并把转换结果放在sin_addr中
	address.sin_port = htons(port);

	ret = bind(listenfd, (struct sockaddr*) & address, sizeof(address));//将socket和地址绑定，称为命名socket
	assert(ret >= 0);

	printf("bind\n");
	ret = listen(listenfd, 5);//把listenfd从主动套接字转化为监听套接字，5为内核拒绝连接请求前待处理的连接队列
	assert(ret >= 0);//完整连接最多5+1个，剩下的半连接状态
	printf("listen\n");


	//创建内核事件表
	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd != -1);

	//将listenfd放在epoll树上
	addfd(epollfd, listenfd, false);//

	//将上述epollfd赋值给http类对象的m_epollfd属性
	http_con::m_epollfd = epollfd;//m_epollfd,内核监听事件表的句柄
	printf("apolladd\n");





	//创建管道套接字
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
	assert(ret != -1);
	
		//设置管道写端为非阻塞，为什么写端要非阻塞？
	setnonblocking(pipefd[1]);
	
		//设置管道读端为ET非阻塞
	addfd(epollfd, pipefd[0], false);
	
		//传递给主循环的信号值，这里只关注SIGALRM和SIGTERM
	addsig(SIGALRM, sig_handler, false);
	addsig(SIGTERM, sig_handler, false);
	
		//循环条件
	bool stop_server = false;
		//常见连接资源数组
		client_data* users_timer = new client_data[MAX_FD];

		//
		//超时标志
		bool timeout = false;



		//每隔TIMESLOT时间触发SIGALRM信号
		alarm(TIMESLOT);

	while (!stop_server)
	{
		printf("epoll_wait1\n");
		//等待所监控描述符上有事件发生
		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER,-1);
		if ((number < 0) && (errno != EINTR))
		{
			printf("epoll falisure\n");
			LOG_ERROR("%s", "epoll failure");//日志
			break;
		}
		printf("epoll_wait nunber is %d\n",number);
		printf("errno is %d\n", errno);


		//对所有就绪事件进行处理
		for (int i = 0; i < number; i++)
		{
			int sockfd = events[i].data.fd;

			//处理新到的客户端连接
			if (sockfd == listenfd)//监听socket有读写，那么就是有新的连接到来。
			{
				//初始化客户端连接地址
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);

				//不同模式？
				//ET模式需要服务器循环读取数据，直到读完
				
				//该连接分配的文件描述符
					int connfd = accept(listenfd, (struct sockaddr*) & client_address,//实际上accept只是从监听队列中取出连接，并不知道连接的网络变化
						&client_addrlength);
					printf("connfd is %d\n", connfd);
					if (connfd < 0)
					{
						printf("errno is: %d\n", errno);
						LOG_ERROR("%s:errno is:%d", "accept error", errno);
						continue;
					}
					if (http_con::m_user_count >= MAX_FD)
					{
						show_error(connfd, "inter server busy");
						LOG_ERROR("%s", "Internal server busy");
						continue;
					}

					//初始化该连接对应的连接资源
					users[connfd].init(connfd, client_address);//大概就是修改http_con对象的数据，地址给放进去，然后注册进内核事件表


					users_timer[connfd].address = client_address;
					users_timer[connfd].sockfd = connfd;

					//创建定时器临时变量
					util_timer* timer = new util_timer;
					//设置定时器对应的连接资源
					timer->user_data = &users_timer[connfd];
					//设置回调函数
					timer->cb_func = cb_func;

					time_t cur = time(NULL);
					//设置绝对超时时间
					timer->expire = cur + 3 * TIMESLOT;
					//创建该连接对应的定时器，初始化为前面的那个临时变量
					users_timer[connfd].timer = timer;

					//将该定时器添加到链表中
					timer_lst.add_timer(timer);
				
			}


			//处理信号,接收到SIGARLM信号，timeout设置为true
			else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
			{
				int sig;
				char signals[1024];

				///从管道读端读出信号值，成功返回字节数，失败返回-1
				//正常情况下，这里的ret返回值总是1，只有14和15两个ASCII码对应的字符
				ret = recv(pipefd[0], signals, sizeof(signals), 0);
				if (ret == -1)
				{
					//handle the error
					continue;
				}
				else if (ret == 0)
				{
					continue;
				}
				else
				{
					//处理信号值对应的逻辑
					for (int i=0 ;i < ret; i++)
					{
						switch (signals[i])
						{
						case SIGALRM:
						{
							timeout = true;
							break;
						}
						case SIGTERM://ctl C发来，触发sigterm信号，停止服务器。本来是默认执行的
						{
							stop_server = true;
						}
						}
					}
				}
			
			}



			//处理异常，关闭连接
			else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
			{
				//users[sockfd].close_con();
				//如果有异常，直接关闭客户端连接,并且移除对应的定时器
				cb_func(&users_timer[sockfd]);


				util_timer* timer = users_timer[sockfd].timer;
				if (timer)
				{
					timer_lst.del_timer(timer);
				}
			}
			//处理客户连接上接收到的数据
			else if (events[i].events & EPOLLIN)
			{

				printf("main_epoll_in\n");
				//根据读的结果，决定是将任务添加到线程池还是关闭连接

				//创建定时器临时变量，将该连接对应的定时器取出来
				util_timer* timer = users_timer[sockfd].timer;


				//读入对应缓冲区
				if (users[sockfd].read()) {

					LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
					Log::get_instance()->flush();
					//若监测到读事件，将该事件放入请求队列
					pool->append(users + sockfd);
					printf("connfd %d has main pool append\n",sockfd);

					////若有数据传输，则将定时器往后延迟3个单位
	               //对其在链表上的位置进行调整
					if (timer)
					{
						time_t cur = time(NULL);
						timer->expire = cur + 3 * TIMESLOT;
						LOG_INFO("%s", "adjust timer once");
						Log::get_instance()->flush();
						timer_lst.adjust_timer(timer);

					}
				}
				else
				{
	
					//users[sockfd].close_con();
					//服务器端关闭，移除对应的定时器。如果read()读取返回0，那么表明对端关闭。这时候关闭连接
					cb_func(&users_timer[sockfd]);
					if (timer)
					{
						timer_lst.del_timer(timer);
					}
				}
			}

			//
			else if (events[i].events & EPOLLOUT)
			{
				//根据写的结果，决定是否关闭连接
				/*if (!users[sockfd].write())
				{
					users[sockfd].close_con();
				}*/
				util_timer* timer = users_timer[sockfd].timer;
				if (!users[sockfd].write())
				{

					LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
					Log::get_instance()->flush();

					////若有数据传输，则将定时器往后延迟3个单位
				   //对其在链表上的位置进行调整
					if (timer)
					{
						time_t cur = time(NULL);
						timer->expire = cur + 3 * TIMESLOT;
						LOG_INFO("%s", "adjust timer once");
						Log::get_instance()->flush();
						timer_lst.adjust_timer(timer);

					}
				}
				else
				{
					//服务器端关闭，移除对应的定时器
					cb_func(&users_timer[sockfd]);
					if (timer)
					{
						timer_lst.del_timer(timer);
					}
				}
			}
			else
			{
				//do nothing
			}
		}
		//处理定时器为非必须事件，收到信号并不是立马处理
		 //完成读写事件后，再进行处理
		if (timeout)
		{
			timer_handler();
			timeout = false;
		}
	}



	close(epollfd);
	close(listenfd);
	delete[]users;
	delete pool;
	return 0;

}
