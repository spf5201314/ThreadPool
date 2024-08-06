#include<stdio.h>
#include"threadpool.h"
#include<iostream>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>

using namespace std;
const int NUMBER = 2;
//任务结构
typedef struct Task
{   void(*function)(void* arg);
    void* arg;
 
}Task;

//线程池结构体

struct ThreadPool
{   
//任务队列
    Task* taskQ;
     int queueCapacity;
     int queueSize;
     int queueFront;
     int queueRear;

//管理者线程
    pthread_t manangeID;
//工作者线程，用数组表示
    pthread_t *threadIDs;
    int minNum;
    int maxNum;
    int busyNum;
    int liveNum;
    int exitNum;
    pthread_mutex_t mutexPool;
    pthread_mutex_t mutexBusy;

    int shutDown; //当前线程池是否工作
    pthread_cond_t notFull;//根据满和空来调用对应的条件变量
    pthread_cond_t notEmpty;

};

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{      ThreadPool* pool = new(ThreadPool);

   do{
        if(pool==NULL)
        {
            cout<<"创建线程池实例失败"<<endl;
            break;    
        }
        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t)*max);
        if(pool->threadIDs==NULL) 
        {
             cout<<"工作者ID创建失败"<<endl;
             break;
        }
        memset(pool->threadIDs,0,sizeof(pthread_t)*max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;//活着的和最小的线程数相等
        pool->exitNum = 0;
        if ( pthread_mutex_init(&pool->mutexPool,NULL) !=0||
             pthread_mutex_init(&pool->mutexBusy,NULL) !=0||
             pthread_cond_init(&pool->notFull,NULL) !=0||
             pthread_cond_init(&pool->notEmpty,NULL) !=0)
        {

            cout<<"mutex初始化没有成功"<<endl;
        }
        /*--------------------------------------*/
        pool->taskQ =(Task*)malloc(sizeof(Task)*queueSize);
        pool->queueCapacity = queueSize;
        pool->queueFront = 0;
        pool->queueRear = 0;
        pool->queueSize = 0;

        pool->shutDown = 0;//创建线程池的时候不能关闭

    //创建线程
        pthread_create(&pool->manangeID,NULL,manager,pool);
        for(int i=0;i<min;++i)
        {
             pthread_create(&pool->threadIDs[i],NULL,worker,pool);
        }
        return pool;
 
   }
   while(0);
    if(pool&&pool->threadIDs) free(pool->threadIDs);
    if(pool&&pool->taskQ) free(pool->taskQ);
    if(pool) delete(pool);
    return NULL;

}

int threadPoolDestroy(ThreadPool*pool)
{   if(pool==NULL)
    { return -1;}
    //关闭线程池
    pool->shutDown=1;
    //阻塞回收管理者
    pthread_join(pool->manangeID,NULL);
    //唤醒阻塞的消费者
    for(int i=0;i<pool->liveNum;++i)
    {
        pthread_cond_signal(&pool->notEmpty);
    }
    //释放内存
     if(pool->threadIDs)
    {
    free(pool->threadIDs);
      }

    if(pool->taskQ)
    {
    free(pool->taskQ);
     }
          pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);
    if(pool)
    {    
     free(pool);
    pool=NULL;
    }
    return 0;
}

int threadPoolBusyNum(ThreadPool*pool)
{   pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}
int threadPoolAliveNum(ThreadPool*pool)
{  pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
 

}
void* worker(void*arg)
{   ThreadPool*pool = (ThreadPool*)arg;
    while(1)
    {   
        pthread_mutex_lock(&pool->mutexPool);
        //判断当前队列是否为空
        while(pool->queueSize==0&&!pool->shutDown)
        {   //阻塞
            pthread_cond_wait(&pool->notEmpty,&pool->mutexPool);//前者为条件变量，后者为对应锁
          
            //判断是不是要删除线程 
            if(pool->exitNum>0)
            {   pool->exitNum--;
                //此处的exit不能放在里面
                if(pool->liveNum>pool->minNum)
                {   pool->liveNum--;
                    
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
            
                }
            }
        }
        //判断线程池是否被关闭了
        if(pool->shutDown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        //移动头结点，作为一个循环队列
        pool->queueFront = (pool->queueFront+1)%pool->queueCapacity;
        pool->queueSize--;

        //任务队列空闲了，可以再次添加任务了
        pthread_cond_signal(&pool->notFull);

        pthread_mutex_unlock(&pool->mutexPool);
        cout<<"线程开始运行了"<<endl;
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);
        
        task.function(task.arg);

        cout<<"线程运行结束了"<<endl;
        free(task.arg);
        task.arg = NULL;
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);



                    
    
    }

    return NULL;

}

void* manager(void*arg)
{   ThreadPool* pool = (ThreadPool*)arg;
    while(!pool->shutDown)
    {//每3s检测一次
        sleep(3);
        //取出线程池中任务的数量和当前线程的数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        //
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);
        //添加线程
        //任务个数》存活线程&&存货线程数《最大数目》
        if((liveNum-busyNum)<queueSize&&liveNum<pool->maxNum)
        {   int counter = 0;
            pthread_mutex_lock(&pool->mutexPool);
            for(int i=0;i<pool->maxNum&&counter<NUMBER&&pool->liveNum<pool->maxNum;++i)
            {   if(pool->threadIDs[i]==0)
                {
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);


        }
        //销毁线程
        //忙的线程*2<存活的线程数&&存活的线程>最小线程
        if(busyNum*2<liveNum&&liveNum>pool->minNum)
        {   pthread_mutex_lock(&pool->mutexPool); 
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool); 
            //让工作的线程自杀
            for(int i=0;i<NUMBER;++i)
            {   pthread_cond_signal(&pool->notEmpty);

    
            }
        }
    }



    return NULL;


}

void threadExit(ThreadPool* pool)
{
    pthread_t tid = pthread_self();
    for(int i=0;i<pool->maxNum;++i)
    {
        if(pool->threadIDs[i]==tid)
        {
            pool->threadIDs[i]=0;
            cout<<"线程删除完毕"<<endl;
        
        }
    }
    pthread_exit(NULL);
}

void threadPoolAdd(ThreadPool*pool,void(*func)(void*arg),void*arg)
{   pthread_mutex_lock(&pool->mutexPool);
    //阻塞生产者线程 任务队列是否满了
    while(pool->queueSize==pool->queueCapacity&&!pool->shutDown)
    {
        pthread_cond_wait(&pool->notFull,&pool->mutexPool);
    }
    
    if(pool->shutDown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    //添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear=(pool->queueRear+1)%pool->queueCapacity;
    pthread_mutex_unlock(&pool->mutexPool);
    pool->queueSize++;
    //添加完任务了，需要让工作者线程开始工作
    pthread_cond_signal(&pool->notEmpty);



}
