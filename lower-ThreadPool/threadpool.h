//创建线程池
typedef struct ThreadPool ThreadPool;
ThreadPool* threadPoolCreate(int min, int max, int queueSize);
//销毁线程池
int threadPoolDestroy(ThreadPool*pool);

//线程池添加任务
void threadPoolAdd(ThreadPool* pool,void(*fun)(void*arg),void*arg);

//获取线程池中工作的个数
int threadPoolBusyNum(ThreadPool*pool);
//
//获取线程池中活着的个数
int threadPoolAliveNum(ThreadPool*pool);

//////////////////////////////
void* worker(void*arg);
void* manager(void*arg);
void threadExit(ThreadPool*pool);
