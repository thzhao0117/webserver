#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>

#include "locker.h"

template<typename T>
class threadpool
{
public:
	threadpool(int thread_number = 8, int max_requests = 1000);
	~threadpool();

	bool append(T* request);//������������������


private:
	static void* worker(void* arg);//��̬��Ա����
	void run();

private:
	int m_thread_number;//�����߳���
	int m_max_requests;//�����������������������Ŀ
	pthread_t* m_threads;//�����̳߳ص����飬��СΪm_thread_number

	std::list<T*> m_workqueue;//�������
	locker m_queuelocker;//����������еĻ�����
	sem m_queuestat;//�Ƿ���������Ҫ����
	bool m_stop;//�Ƿ�����߳�

};
//��������
template<typename T>
threadpool<T>::~threadpool()
{
	delete[] m_threads;
	m_stop = true;
}

//Ĭ�Ϲ��캯��
template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
	m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL)
	//:֮���ǹ��캯���ĳ�ʼ���б���ʽ��   ������ֵ����������ֵ��
{
	if ((thread_number <= 0) || (max_requests <= 0))
	{
		throw std::exception();
	}
	//�߳�id��ʼ��
	m_threads = new pthread_t(m_thread_number);
	if (!m_threads)
	{
		throw std::exception();
	}

	for (int i = 0; i < thread_number; ++i)
	{
		printf("create the %dth thread\n", i);
		//worker,�൱���߳�main�����Ŀ�ʼ��ַ��this����worker�����Ʋ���arg.������
		//thisָ��������Ķ�����Ϊ���ö���ĳ�Ա������Ĭ�ϲ�������
		//ѭ�������̣߳����Ұ��չ����߳�Ҫ���������
		if (pthread_create(m_threads + i, NULL, worker, this) != 0)
		{
			delete[] m_threads;
			throw std::exception();
		}
		//���߳̽��з���󣬾Ͳ���Ҫ���������߳���
		//�߳��ʼ������Ϊjoinable����������Ҫ�Լ���ʾ�Ļ��ա�����Ϊdetached֮���ڴ���Դ��������ֹʱ��ϵͳ�Զ��ͷ�
		if (pthread_detach(m_threads[i]))
		{
			delete[] m_threads;
			throw std::exception();
		}
	}
}

//�̴߳�����
//�ڲ�����˽�г�Ա����run(),����̵߳Ĵ���
template<typename T>
void* threadpool<T>::worker(void* arg)
{
	//������ǿתΪ�̳߳��࣬���ó�Ա����
	threadpool* pool = (threadpool*)arg;
	pool->run();
	return pool;
}


//������������������

//ͨ��list��������������У�����������ʱ��
//ͨ����������֤�̰߳�ȫ�������ɺ�ͨ���ź�������������Ҫ�������ע���߳�ͬ����
template<typename T>
bool threadpool<T>::append(T* request)
{
	//������������һ��Ҫ����
	m_queuelocker.lock();
	// ����Ӳ����Ԥ������������е����ֵ
	if (m_workqueue.size() > m_max_requests)
	{
		m_queuelocker.unlock();
		return false;
	}

	//�������
	m_workqueue.push_back(request);
	m_queuelocker.unlock();

	//�ź�������������Ҫ����
	m_queuestat.post();
	return true;
}

//runִ������
//��Ҫʵ�֣������̴߳����������ȡ��ĳ��������д���ע���߳�ͬ����
template<typename T>
void threadpool<T>::run()
{
	while (!m_stop)
	{
		//�ź����ȴ���
		//sem_wait()ͨ��ԭ�Ӳ������ź���ֵ��1��sem_post��1�����ź�����ֵ����0ʱ����������sem_wait�ȴ��ź������߳̽��ᱻ����

		m_queuestat.wait();
		//�����Ѻ��ȼ��ϻ�����
		m_queuelocker.lock();

		if (m_workqueue.empty())
		{
			m_queuelocker.unlock();
			continue;
		}

		//�����������ȡ����һ������
		//��������������ɾ��
		T* request = m_workqueue.front();
		m_workqueue.pop_front();

		m_queuelocker.unlock();

		
		if (!request)
		{
			continue;
		}
		//process(ģ�����еķ���,������http��)���д���
		request->process();
	}
}



#endif