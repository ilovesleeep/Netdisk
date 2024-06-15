#include "thread_pool.h"
#include <pthread.h>


int queueInit(task_queue_t * que)
{
    if(que) {
        que->pFront = NULL;
        que->pRear = NULL;
        que->queSize = 0;
        que->flag = 1;
        int ret = pthread_mutex_init(&que->mutex, NULL);
        THREAD_ERROR_CHECK(ret, "pthread_mutex_init");

        ret = pthread_cond_init(&que->cond, NULL);
        THREAD_ERROR_CHECK(ret, "pthread_cond_init");
    }
    return 0;
}

int queueDestroy(task_queue_t * que)
{
    if(que) {
        int ret = pthread_mutex_destroy(&que->mutex);
        THREAD_ERROR_CHECK(ret, "pthread_mutex_destroy");

        ret = pthread_cond_destroy(&que->cond);
        THREAD_ERROR_CHECK(ret, "pthread_cond_destroy");
    }
    return 0;
}

int queueIsEmpty(task_queue_t * que)
{
    return que->queSize == 0;
}

int taskSize(task_queue_t * que)
{
    return que->queSize;
}

int taskEnque(task_queue_t * que, task_t * ptask)
{
    int ret = pthread_mutex_lock(&que->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
    if(queueIsEmpty(que)) {
        que->pFront = que->pRear = ptask;
    } else {//不为空
        que->pRear->pNext = ptask;
        que->pRear = ptask;
    }
    que->queSize++;
    ret = pthread_mutex_unlock(&que->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
    //通知消费者取任务
    ret = pthread_cond_signal(&que->cond);
    THREAD_ERROR_CHECK(ret, "pthread_cond_signal");
    return 0;
}

//获取一个任务
task_t * taskDeque(task_queue_t * que)
{
    int ret = pthread_mutex_lock(&que->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_lock");
    task_t * pret;
    while(que->flag && queueIsEmpty(que)) {//虚假唤醒
        pthread_cond_wait(&que->cond, &que->mutex);
    }
    if(que->flag) {
        //元素出队操作
        pret = que->pFront;
        if(taskSize(que) == 1) {
            que->pFront = que->pRear = NULL;
        } else {
            que->pFront = que->pFront->pNext;
        }
        que->queSize--;
    } else {
        pret = NULL;
    }
    ret = pthread_mutex_unlock(&que->mutex);
    THREAD_ERROR_CHECK(ret, "pthread_mutex_unlock");
    return pret;
}

//主线程调用
int broadcastALL(task_queue_t * que)
{
    //先修改要退出的标识位
    que->flag = 0;
    int ret = pthread_cond_broadcast(&que->cond);
    THREAD_ERROR_CHECK(ret, "pthread_cond_broadcast");

    return 0;
}
