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

	bool append(T* request);//往请求队列中添加任务


private:
	static void* worker(void* arg);//静态成员函数
	void run();

private:
	int m_thread_number;//池中线程数
	int m_max_requests;//请求队列中允许的最大请求数目
	pthread_t* m_threads;//描述线程池的数组，大小为m_thread_number

	std::list<T*> m_workqueue;//请求队列
	locker m_queuelocker;//保护请求队列的互斥锁
	sem m_queuestat;//是否有任务需要处理
	bool m_stop;//是否结束线程

};
//析构函数
template<typename T>
threadpool<T>::~threadpool()
{
	delete[] m_threads;
	m_stop = true;
}

//默认构造函数
template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
	m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL)
	//:之后是构造函数的初始化列表，形式是   变量（值），变量（值）
{
	if ((thread_number <= 0) || (max_requests <= 0))
	{
		throw std::exception();
	}
	//线程id初始化
	m_threads = new pthread_t(m_thread_number);
	if (!m_threads)
	{
		throw std::exception();
	}

	for (int i = 0; i < thread_number; ++i)
	{
		printf("create the %dth thread\n", i);
		//worker,相当于线程main函数的开始地址，this就是worker的形势参数arg.来传递
		//this指向调用它的对象，作为调用对象的成员函数的默认参数传入
		//循环创造线程，并且按照工作线程要求进行运行
		if (pthread_create(m_threads + i, NULL, worker, this) != 0)
		{
			delete[] m_threads;
			throw std::exception();
		}
		//将线程进行分离后，就不需要单独回收线程了
		//线程最开始被创建为joinable，但是这需要自己显示的回收。设置为detached之后，内存资源可以在终止时由系统自动释放
		if (pthread_detach(m_threads[i]))
		{
			delete[] m_threads;
			throw std::exception();
		}
	}
}

//线程处理函数
//内部访问私有成员函数run(),完成线程的处理
template<typename T>
void* threadpool<T>::worker(void* arg)
{
	//将参数强转为线程池类，调用成员方法
	threadpool* pool = (threadpool*)arg;
	pool->run();
	return pool;
}


//向请求队列中添加任务

//通过list容器创建请求队列，向队列中添加时，
//通过互斥锁保证线程安全，添加完成后通过信号量提醒有任务要处理，最后注意线程同步。
template<typename T>
bool threadpool<T>::append(T* request)
{
	//操作工作队列一定要加锁
	m_queuelocker.lock();
	// 根据硬件，预先设置请求队列的最大值
	if (m_workqueue.size() > m_max_requests)
	{
		m_queuelocker.unlock();
		return false;
	}

	//添加任务
	m_workqueue.push_back(request);
	m_queuelocker.unlock();

	//信号量提醒有任务要处理
	m_queuestat.post();
	return true;
}

//run执行任务
//主要实现，工作线程从请求队列中取出某个任务进行处理，注意线程同步。
template<typename T>
void threadpool<T>::run()
{
	while (!m_stop)
	{
		//信号量等待。
		//sem_wait()通过原子操作将信号量值减1，sem_post加1，当信号量的值大于0时候，其他调用sem_wait等待信号量的线程将会被唤醒

		m_queuestat.wait();
		//被唤醒后先加上互斥锁
		m_queuelocker.lock();

		if (m_workqueue.empty())
		{
			m_queuelocker.unlock();
			continue;
		}

		//从请求队列中取出第一个任务
		//将任务从请求队列删除
		T* request = m_workqueue.front();
		m_workqueue.pop_front();

		m_queuelocker.unlock();

		
		if (!request)
		{
			continue;
		}
		//process(模板类中的方法,这里是http类)进行处理
		request->process();
	}
}



#endif