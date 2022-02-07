
#ifndef LST_TIMER
#define LST_TIMER

#include<arpa/inet.h>
#include <time.h>
#include<stdio.h>



#include "http_con.h"
//#include "../log/log.h"

class util_timer;
//连接资源
struct client_data
{
    //客户端socket地址
    sockaddr_in address;
    //socket文件描述符
    int sockfd;
    //定时器
    util_timer* timer;
};

//定时器类
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    
    //超时时间
    time_t expire;
    //回调函数
    void (*cb_func)(client_data*);
    //连接资源
    client_data* user_data;
    //前向定时器
    util_timer* prev;
    //后继定时器
    util_timer* next;
};




//定时器容器类


/*项目中的定时器容器为带头尾结点的升序双向链表，具体的为每个连接创建一个定时器，将其添加到链表中，并按照超时时间升序排列。执行定时任务时，将到期的定时器从链表中删除。

从实现上看，主要涉及双向链表的插入，删除操作，其中添加定时器的事件复杂度是O(n),删除定时器的事件复杂度是O(1)。

升序双向链表主要逻辑如下，具体的，

创建头尾节点，其中头尾节点没有意义，仅仅统一方便调整

add_timer函数，将目标定时器添加到链表中，添加时按照升序添加

若当前链表中只有头尾节点，直接插入

否则，将定时器按升序插入

adjust_timer函数，当定时任务发生变化,调整对应定时器在链表中的位置

客户端在设定时间内有数据收发,则当前时刻对该定时器重新设定时间，这里只是往后延长超时时间

被调整的目标定时器在尾部，或定时器新的超时值仍然小于下一个定时器的超时，不用调整

否则先将定时器从链表取出，重新插入链表

del_timer函数将超时的定时器从链表中删除

常规双向链表删除结点

*/
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();
    
    void add_timer(util_timer* timer);
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);
    void tick();

private:
    ////私有成员，被公有成员add_timer和adjust_time调用
    void add_timer(util_timer* timer, util_timer* lst_head);


    //头尾节点
    util_timer* head;
    util_timer* tail;
};





#endif