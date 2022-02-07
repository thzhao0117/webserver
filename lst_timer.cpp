#include"lst_timer.h"




//���캯��
sort_timer_lst::sort_timer_lst() 
{
    head = NULL;
    tail = NULL;
}
//������������
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

//��Ӷ�ʱ�����ڲ�����˽�г�Աadd_timer
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

    ////����µĶ�ʱ����ʱʱ��С�ڵ�ǰͷ�����
   //ֱ�ӽ���ǰ��ʱ�������Ϊͷ�����
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        printf("add_timer for connfd \n");
        return;
    }
    //�������˽�г�Ա�������ڲ��ڵ�
    add_timer(timer, head);
}

////������ʱ�����������仯ʱ��������ʱ���������е�λ��
void sort_timer_lst::adjust_timer(util_timer* timer)
{
    if (!timer)
    {
        return;
    }
    util_timer* tmp = timer->next;


    //�������Ķ�ʱ��������β��
   //��ʱ����ʱֵ��ȻС����һ����ʱ����ʱֵ��������
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }

    //��������ʱ��������ͷ��㣬����ʱ��ȡ�������²���
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    //��������ʱ�����ڲ�������ʱ��ȡ�������²���
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

//ɾ����ʱ��
void sort_timer_lst::del_timer(util_timer* timer)
{
    if (!timer)
    {
        return;
    }
    ////������ֻ��һ����ʱ������Ҫɾ���ö�ʱ��
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    ////��ɾ���Ķ�ʱ��Ϊͷ���
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    // //��ɾ���Ķ�ʱ��Ϊβ���
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    //��ɾ���Ķ�ʱ���������ڲ�������������ɾ��
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

//��Ҫ���ڵ��������ڲ����
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
    util_timer* prev = lst_head;
    util_timer* tmp = prev->next;

    //������ǰ���֮����������ճ�ʱʱ���ҵ�Ŀ�궨ʱ����Ӧ��λ�ã�����˫������������

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

    ///�����귢�֣�Ŀ�궨ʱ����Ҫ�ŵ�β��㴦
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}



/*ʹ��ͳһ�¼�Դ��SIGALRM�ź�ÿ�α���������ѭ���е���һ�ζ�ʱ�����������������������е��ڵĶ�ʱ����

������߼����£�

������ʱ������������������ͷ��㿪ʼ���δ���ÿ����ʱ����ֱ��������δ���ڵĶ�ʱ��

����ǰʱ��С�ڶ�ʱ����ʱʱ�䣬����ѭ������δ�ҵ����ڵĶ�ʱ��

����ǰʱ����ڶ�ʱ����ʱʱ�䣬���ҵ��˵��ڵĶ�ʱ����ִ�лص�������Ȼ������������ɾ����Ȼ���������*/
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    printf("timer tick\n");
    // LOG_INFO("%s", "timer tick");
    // Log::get_instance()->flush();

     //��ȡ��ǰʱ��
    time_t cur = time(NULL);//time() ��ָ������ Unix ��Ԫ��January 1 1970 00:00:00 GMT����ĵ�ǰʱ��������ĺ�������Ҫ������ȡ��ǰ��ϵͳʱ�䣬���صĽ����һ��time_t����
    util_timer* tmp = head;

    ////������ʱ������
    while (tmp)
    {
        ///��������Ϊ��������
       //��ǰʱ��С�ڶ�ʱ���ĳ�ʱʱ�䣬����Ķ�ʱ��Ҳû�е���
        if (cur < tmp->expire)
        {
            break;
        }
        //��ǰ��ʱ�����ڣ�����ûص�������ִ�ж�ʱ�¼�
        tmp->cb_func(tmp->user_data);
        ////�������Ķ�ʱ��������������ɾ����������ͷ���
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}
