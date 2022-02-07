
#ifndef LST_TIMER
#define LST_TIMER

#include<arpa/inet.h>
#include <time.h>
#include<stdio.h>



#include "http_con.h"
//#include "../log/log.h"

class util_timer;
//������Դ
struct client_data
{
    //�ͻ���socket��ַ
    sockaddr_in address;
    //socket�ļ�������
    int sockfd;
    //��ʱ��
    util_timer* timer;
};

//��ʱ����
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    
    //��ʱʱ��
    time_t expire;
    //�ص�����
    void (*cb_func)(client_data*);
    //������Դ
    client_data* user_data;
    //ǰ��ʱ��
    util_timer* prev;
    //��̶�ʱ��
    util_timer* next;
};




//��ʱ��������


/*��Ŀ�еĶ�ʱ������Ϊ��ͷβ��������˫�����������Ϊÿ�����Ӵ���һ����ʱ����������ӵ������У������ճ�ʱʱ���������С�ִ�ж�ʱ����ʱ�������ڵĶ�ʱ����������ɾ����

��ʵ���Ͽ�����Ҫ�漰˫������Ĳ��룬ɾ��������������Ӷ�ʱ�����¼����Ӷ���O(n),ɾ����ʱ�����¼����Ӷ���O(1)��

����˫��������Ҫ�߼����£�����ģ�

����ͷβ�ڵ㣬����ͷβ�ڵ�û�����壬����ͳһ�������

add_timer��������Ŀ�궨ʱ����ӵ������У����ʱ�����������

����ǰ������ֻ��ͷβ�ڵ㣬ֱ�Ӳ���

���򣬽���ʱ�����������

adjust_timer����������ʱ�������仯,������Ӧ��ʱ���������е�λ��

�ͻ������趨ʱ�����������շ�,��ǰʱ�̶Ըö�ʱ�������趨ʱ�䣬����ֻ�������ӳ���ʱʱ��

��������Ŀ�궨ʱ����β������ʱ���µĳ�ʱֵ��ȻС����һ����ʱ���ĳ�ʱ�����õ���

�����Ƚ���ʱ��������ȡ�������²�������

del_timer��������ʱ�Ķ�ʱ����������ɾ��

����˫������ɾ�����

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
    ////˽�г�Ա�������г�Աadd_timer��adjust_time����
    void add_timer(util_timer* timer, util_timer* lst_head);


    //ͷβ�ڵ�
    util_timer* head;
    util_timer* tail;
};





#endif