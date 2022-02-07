#include"lst_timer.h"




//构造函数
sort_timer_lst::sort_timer_lst() 
{
    head = NULL;
    tail = NULL;
}
//常规销毁链表
sort_timer_lst::~sort_timer_lst()
{
    util_timer* tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

//添加定时器，内部调用私有成员add_timer
void sort_timer_lst::add_timer(util_timer* timer)
{
    if (!timer)
    {
        return;
    }
    if (!head)
    {
        head = tail = timer;
        return;
    }

    ////如果新的定时器超时时间小于当前头部结点
   //直接将当前定时器结点作为头部结点
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        printf("add_timer for connfd \n");
        return;
    }
    //否则调用私有成员，调整内部节点
    add_timer(timer, head);
}

////调整定时器，任务发生变化时，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer* timer)
{
    if (!timer)
    {
        return;
    }
    util_timer* tmp = timer->next;


    //被调整的定时器在链表尾部
   //定时器超时值仍然小于下一个定时器超时值，不调整
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }

    //被调整定时器是链表头结点，将定时器取出，重新插入
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    //被调整定时器在内部，将定时器取出，重新插入
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

//删除定时器
void sort_timer_lst::del_timer(util_timer* timer)
{
    if (!timer)
    {
        return;
    }
    ////链表中只有一个定时器，需要删除该定时器
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    ////被删除的定时器为头结点
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    // //被删除的定时器为尾结点
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    //被删除的定时器在链表内部，常规链表结点删除
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

//主要用于调整链表内部结点
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
    util_timer* prev = lst_head;
    util_timer* tmp = prev->next;

    //遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作

    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    ///遍历完发现，目标定时器需要放到尾结点处
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}



/*使用统一事件源，SIGALRM信号每次被触发，主循环中调用一次定时任务处理函数，处理链表容器中到期的定时器。

具体的逻辑如下，

遍历定时器升序链表容器，从头结点开始依次处理每个定时器，直到遇到尚未到期的定时器

若当前时间小于定时器超时时间，跳出循环，即未找到到期的定时器

若当前时间大于定时器超时时间，即找到了到期的定时器，执行回调函数，然后将它从链表中删除，然后继续遍历*/
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    printf("timer tick\n");
    // LOG_INFO("%s", "timer tick");
    // Log::get_instance()->flush();

     //获取当前时间
    time_t cur = time(NULL);//time() 是指返回自 Unix 纪元（January 1 1970 00:00:00 GMT）起的当前时间的秒数的函数，主要用来获取当前的系统时间，返回的结果是一个time_t类型
    util_timer* tmp = head;

    ////遍历定时器链表
    while (tmp)
    {
        ///链表容器为升序排列
       //当前时间小于定时器的超时时间，后面的定时器也没有到期
        if (cur < tmp->expire)
        {
            break;
        }
        //当前定时器到期，则调用回调函数，执行定时事件
        tmp->cb_func(tmp->user_data);
        ////将处理后的定时器从链表容器中删除，并重置头结点
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}
