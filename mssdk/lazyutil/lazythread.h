/****************************************************************************************************************
 * filename     sv_thread.h
 * describe     thread can be simple create by inherit this class
 * author       Created by dawson on 2019/04/19
 * Copyright    ?2007 - 2029 Sunvally. All Rights Reserved.
 ***************************************************************************************************************/

#ifndef _SV_THREAD_H_
#define _SV_THREAD_H_
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <mutex>
#include <future>
#include <chrono>
#include <thread>
#include <string>
#include <queue>
//#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
//#include <types.h>
//#include <timeb.h>
#include "lazylog.h"
//#include "lazylock.h"
//#include "lazymicro.h"
using namespace std;
//#define ENABLE_LAZY_EVENT
#ifdef ENABLE_LAZY_EVENT
#include "LazyEvent.hpp"
#endif
#define THREAD_RETURN void*
typedef THREAD_RETURN (*routine_pt)(void *);
//#define ENABLE_PTHREAD

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef lbtrace
#define lbtrace printf
#endif
#ifndef lberror
#define lberror
#endif

class LazyThread
{
public:
    LazyThread(const char *pname = NULL)
    {
#ifdef ENABLE_PTHREAD
        memset(&m_Tid, 0, sizeof(m_Tid));
#else
        m_thread = NULL;
#endif
        m_bRun      = 0;
        m_nPriority = 0;
        m_bSystem   = 0;
        m_bExit		= 0;
        m_utid      = -1;
        SetThreadName(pname);
        lbtrace("SetThreadName end");
        lbtrace("LazyThread end");
    }

    virtual ~LazyThread()
    {
        Stop();
#ifndef ENABLE_PTHREAD
        if(m_thread)
        {
            delete m_thread;
            m_thread = NULL;
        }
#endif
    }

    virtual int ThreadProc() = 0;

    virtual int OnThreadStart()
    {
        return 0;
    }

    virtual void OnThreadStop() {}

    virtual int Run(int priority = 0, int bsystem = 0)
    {
        int ret = 0;
        lock_guard<recursive_mutex> lock(m_mutex);
        if(!m_bRun)
        {
            ret = Create(priority, bsystem);
            m_bRun = 1;
        }

        return ret;
    }

    int Stop()
    {
        lock_guard<recursive_mutex> lock(m_mutex);
        if(m_bRun)
        {
            m_bRun = 0;
            //#ifdef ENABLE_PTHREAD
            //lbtrace("before stop wait...\n");
            for (int i = 0; !m_bExit; i++)
            {
                ThreadSleep(20);
                if (i % 200 == 100)
                {
                    lbtrace("thread:%s sleep for %d ms, m_bRun:%d\n", m_thread_name.c_str(), i * 20, m_bRun);
                }
            }
            //pthread_join(m_Tid, NULL);
            /*#else
                        if (m_pthread)
                        {
                            lbtrace("before pthread join\n");
                            m_pthread->join();
                            lbtrace("after pthread join\n");
                        }
            #endif*/
        }
#ifdef ENABLE_PTHREAD
        memset(&m_Tid, 0, sizeof(m_Tid));
#endif
        return 0;
    }

    bool IsAlive()
    {
        return m_bRun;
    }

    void ThreadSleep(int ms)
    {
#ifdef _MSC_VER
        Sleep(ms);
#else
        usleep(ms * 1000);
#endif
    }

    void SetThreadName(const char *pname)
    {
        lock_guard<recursive_mutex> lock(m_mutex);
        if(pname)
        {
            m_thread_name = pname;
            lbtrace("SetThreadName:%s", m_thread_name.c_str());
        }
        else
        {
            m_thread_name.clear();
        }
    }

    unsigned int GetTid()
    {
        return m_utid;//m_pthread ? *(unsigned int*)&m_pthread->get_id() : -1;
    }
private:

#ifdef ENABLE_PTHREAD
    int Create(int priority = 0, int bsystem = 0)
    {
        int res = 0;
        if(!m_bRun)
        {
            pthread_attr_t tattr;
            m_bSystem = bsystem;
            m_nPriority = priority;

            res = pthread_attr_init(&tattr);

            if (0 != res)
            {
                /* error */
                //CloseDebug("pthread_attr_init error\n");
                printf("res:%d = pthread_attr_init(&tattr) faild\n", res);
                return res;
            }
            res = pthread_attr_setstacksize(&tattr, 64 * 1024);
            if (0 != res)
            {
                /* error */
                //CloseDebug("pthread_attr_init error\n");
                printf("res:%d = pthread_attr_setstacksize(&tattr, 64*1024) faild\n", res);
                return res;
            }
            // 设置为内核级的线程，以获取较高的响应速度
            if(bsystem)
            {
                res = pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
                //pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
                if (res != 0)
                {
                    printf("res:%d = pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM) failed\n", res);
                    return res;
                }
            }

            res = pthread_create(&m_Tid, &tattr, InitialThreadProc, this);
            if(0 != res)
            {
                printf("res:%d = pthread_create(&m_Tid, &tattr, InitialThreadProc, this) failed\n", res);
                return res;
            }

            res = pthread_attr_destroy(&tattr);
            if(0 != res)
            {
                printf("res:%d = pthread_attr_destroy(&tattr) failed\n", res);
                return res;
            }

            //m_utid = m_Tid;
            return res;
        }
        else
        {
            //printf("thread already had been create!, m_bRun:%lu\n", m_bRun);
        }

        return res;
    }

    static THREAD_RETURN InitialThreadProc(void *arg)
    {
        //int ret = -1;
        LazyThread *pThread = static_cast<LazyThread *>(arg);
        if(pThread)
        {
            pThread->m_bRun = 1;
            pThread->m_bExit = 0;
#ifdef ENABLE_LAZY_EVENT
            pThread->m_thread_event.reset();
#endif
            int ret = pThread->OnThreadStart();
            if(ret == 0)
            {
                pThread->ThreadProc();
            }
            pThread->OnThreadStop();
            pthread_detach(pthread_self());
            pThread->m_bRun = 0;
            pThread->m_bExit = 1;
#ifdef ENABLE_LAZY_EVENT
            pThread->m_thread_event.post();
#endif
        }

        return NULL;
    }
#else
    int Create(int priority = 0, int bsystem = 0)
    {
        if (!m_bRun)
        {
            m_thread = new thread(LazyThread::InitThreadProc, this);
            m_thread->get_id();
            //m_utid = *(unsigned int*)&tid;
            m_thread->detach();
            return 0;
        }

        return -1;
    }

    static unsigned long CurrentThreadId()
    {
#ifdef _WIN32
        return (unsigned long int)GetCurrentThreadId();
#else
        return pthread_self();
#endif
    }

    static void InitThreadProc(void *arg)
    {
        LazyThread *pthis = static_cast<LazyThread *>(arg);
        assert(pthis);
        pthis->m_bRun = 1;
        pthis->m_utid = CurrentThreadId();
        if(pthis->m_thread_name.empty())
        {
            char buf[256];
            snprintf(buf, 256, "%lu", pthis->m_utid);
            pthis->m_thread_name = buf;
        }
        lbtrace("thread:%s begin proc\n", pthis->m_thread_name.c_str());
        int ret = pthis->OnThreadStart();
        if(ret == 0)
        {
            pthis->ThreadProc();
        }
        pthis->OnThreadStop();
        pthis->m_bRun = 0;
        lbtrace("thread:%s end proc\n", pthis->m_thread_name.c_str());
        pthis->m_bExit = 1;
    }
#endif

protected:
    int		    	m_bRun;
    int         	m_nPriority;
    int         	m_bSystem;
    int			    m_bExit;
    unsigned long   m_utid;
    std::recursive_mutex  m_mutex;
    //ILazyMutex     *m_pmutex;
    string          m_thread_name;
    thread         *m_thread;
};

template<class T>
class LazyThreadEx: public LazyThread
{
public:
    LazyThreadEx(const char *pname = NULL): LazyThread(pname)
    {
        m_powner		= NULL;
        m_pcallback_proc	= NULL;
    }

    ~LazyThreadEx()
    {
    }

    typedef int (T::*callback)();

    virtual int init_class_func(T *powner, callback class_func)
    {
        m_powner = powner;
        m_pcallback_proc = class_func;

        return 0;
    }

    virtual int ThreadProc()
    {
        int ret = -1;
        //printf("LazyThreadEx::ThreadProc begin");
        if(m_pcallback_proc && m_powner)
        {
            //printf("before ret = (m_powner:%p->*m_pcallback_proc:%p)()\n", m_powner, m_pcallback_proc);
            ret = (m_powner->*m_pcallback_proc)();
            //printf("ret:%d = (m_powner->*m_pcallback_proc)()\n", ret);
        }

        return ret;
    }

protected:
    callback		m_pcallback_proc;
    T				*m_powner;
};

template<class T>
class LazyTimer: public LazyThread
{
protected:
    int64_t interval_ms_;
    int64_t last_timestamp_;
    int64_t sleep_time_;
    typedef int (T::*callback)();
    callback		ptimer_cb_;
    T				*powner_;
    bool    bexec_immediately_;

public:
    LazyTimer(int64_t interval_ms)
    {
        interval_ms_ = interval_ms;
        last_timestamp_ = ms_timestamp();
        bexec_immediately_ = false;
        ptimer_cb_ = NULL;
        powner_ = NULL;
        sleep_time_ = 20;
    }
    virtual ~LazyTimer() {}

    virtual int ThreadProc()
    {
        sleep_time_ = interval_ms_ > 20 ? 20 : interval_ms_;
        last_timestamp_ = ms_timestamp();
        while(IsAlive() && ptimer_cb_ && powner_)
        {
            if((int64_t)ms_timestamp() - last_timestamp_ < interval_ms_ && !bexec_immediately_)
            {
                ThreadSleep(sleep_time_);
                continue;
            }
            last_timestamp_ = ms_timestamp();
            (powner_->*ptimer_cb_)();
            bexec_immediately_ = false;
        };

        return 0;
    }

    virtual int Start(T *powner, callback class_func)
    {
        powner_ = powner;
        ptimer_cb_ = class_func;

        Run();
        return 0;
    }

    virtual void exec_immediately()
    {
        bexec_immediately_ = true;
    }

    virtual int SetTimerInterval(int interval_ms)
    {
        interval_ms_ = interval_ms;
        sleep_time_ = interval_ms_ > 20 ? 20 : interval_ms;
        return 0;
    }

    static uint64_t ms_timestamp()
    {
        struct timeb tb;
        ftime(&tb);
        return tb.time * 1000 + tb.millitm;
    }
};
class TaskMsg
{
public:
    virtual ~TaskMsg() {}
    virtual void Run() = 0;
};

template <class FunctorT>
class ThreadTaskMsg:public TaskMsg{
public:
    void Run()
    {
        func(val);
    }
public:
    string val;
    FunctorT func;
};
class MessageData {
public:
    MessageData() {}
    virtual ~MessageData() {}
};
class MessageLikeTask : public MessageData {
public:
    virtual void Run() = 0;
};

template <class FunctorT>
class MessageWithFunctor final : public MessageLikeTask {
public:
    explicit MessageWithFunctor(FunctorT&& functor)
        : functor_(std::forward<FunctorT>(functor)) {}

    void Run() override { functor_(); }
    ~MessageWithFunctor() override {}
private:
    

    typename std::remove_reference<FunctorT>::type functor_;

 //   RTC_DISALLOW_COPY_AND_ASSIGN(MessageWithFunctor);
};

#endif /* sv_thread.h */
