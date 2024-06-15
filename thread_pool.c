#include "thread_pool.h"
#include <bits/pthreadtypes.h>

//每一个子线程在执行的函数执行体(start_routine)
void * threadFunc(void* arg)
{
    //不断地从任务队列中获取任务，并执行
    threadpool_t * pThreadpool = (threadpool_t*)arg;
    while(1) {
        task_t * ptask = taskDeque(&pThreadpool->que);
        if(ptask) {
            //执行业务逻辑
            doTask(ptask);
            free(ptask);//执行完任务后，释放任务节点
        } else {//ptask为NULL的情况
            break;
        }
    }
    printf("sub thread %ld is exiting.\n", pthread_self());
    return NULL;
}


int threadpoolInit(threadpool_t * pthreadpool, int num)
{
    pthreadpool->pthreadNum = num;
    pthreadpool->pthreads = calloc(num, sizeof(pthread_t));
    queueInit(&pthreadpool->que);

    return 0;
}

int threadpoolDestroy(threadpool_t * pthreadpool)
{
    free(pthreadpool->pthreads);
    queueDestroy(&pthreadpool->que);
    return 0;
}

int threadpoolStart(threadpool_t * pthreadpool)
{
    if(pthreadpool) {
        for(int i = 0; i < pthreadpool->pthreadNum; ++i) {
            int ret = pthread_create(&pthreadpool->pthreads[i],
                           NULL,
                           threadFunc, pthreadpool);
            THREAD_ERROR_CHECK(ret, "pthread_create");
        }
    }
    return 0;
}

int threadpoolStop(threadpool_t * pthreadpool)
{
    //如果任务队列中还有任务，先等待一下，所有任务执行完毕之后
    //再发广播，退出
    while(!queueIsEmpty(&pthreadpool->que))
    {
        sleep(1);//每一个线程都可以sleep
    }
    
    //发广播, 通知所有的子线程退出
    broadcastALL(&pthreadpool->que);
    //回收所有子线程的资源
    for(int i = 0; i < pthreadpool->pthreadNum; ++i) {
        pthread_join(pthreadpool->pthreads[i], NULL);
    }
    return 0;
}

