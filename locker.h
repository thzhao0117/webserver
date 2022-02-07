/*#ifndef x //先测试x是否被宏定义过

 
　　#define x

 
　　程序段1 //如果x没有被宏定义过，定义x，并编译程序段1

 
　　#endif

 
　　程序段2 //如果x已经定义过了则编译程序段2的语句，“忽视”程序段1。

 
　　条件指示符#ifndef 的最主要目的是防止头文件的重复包含和编译*/
#ifndef LOCKER_H
#define LOCKER_H

#include<exception>
#include<pthread.h>
#include<semaphore.h>

/*封装信号量的类*/
class sem
{
public:
    sem()//构造函数。创建并初始化信号量
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }

    sem(int num)
        {
            if (sem_init(&m_sem, 0, num) != 0)
            {
                throw std::exception();
            }
        }

    

    //销毁信号量
    ~sem()
    {
        sem_destroy(&m_sem);
    }

    //等待信号量
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    //增加信号量
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

//封装互斥锁类

class locker
{
public:
    locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }

    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    /*获取锁*/
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex)==0;
    }
    /*释放锁*/
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t* get()
    {
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};






/*
class Mutex
{
public:
    Mutex()
    {
        if (pthread_mutex_init(&mutex_, nullptr) != 0)
        {
            throw std::exception();
        }

    }
    ~Mutex()
    {
        pthread_mutex_destroy(&mutex_);
    }
    bool Lock()
    {
        return pthread_mutex_lock(&mutex_) == 0;
    }
    bool Unlock()
    {
        return pthread_mutex_unlock(&mutex_) == 0;
    }

    pthread_mutex_t* get()
    {
        return &mutex_;
    }

private:
    pthread_mutex_t mutex_;
};

*/

//本来的代码

/*

class cond
{
public:
    cond()
    {
        if (pthread_mutex_init(&mutex_, nullptr) != 0)
        {
            throw std::exception();
        }
        if (pthread_cond_init(&cond_, nullptr) != 0)
        {
            // 
            pthread_mutex_destroy(&mutex_);
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&cond_);
    };
    bool Wait(pthread_mutex_t* m_mutex)
    {
        int ret = 0;
        //pthread_mutex_lock(&mutex_);
        ret = pthread_cond_wait(&cond_, &mutex_);
        //pthread_mutex_unlock(&mutex_);
        return ret == 0;
    }

    bool timewait(pthread_mutex_t* m_mutex, struct timespec t)
    {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&cond_, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool Signal()
    {
        return pthread_cond_signal(&cond_) == 0;
    }

    bool Broadcast()
    {
        return pthread_cond_broadcast(&cond_) == 0;
    }

private:
    pthread_cond_t cond_;
    pthread_mutex_t mutex_;
};

*/

class cond
{
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            //pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    bool Wait(pthread_mutex_t* m_mutex)
    {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, m_mutex);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t* m_mutex, struct timespec t)
    {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool Signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool Broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    //static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};
#endif
