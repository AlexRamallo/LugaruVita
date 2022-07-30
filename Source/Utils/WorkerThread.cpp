#include "Utils/WorkerThread.hpp"
#include "Utils/Log.h"

#include "Thirdparty/microprofile/microprofile.h"
#include "Objects/Person.hpp"

#include <vector>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>

#ifndef NDEBUG
	#define PTCHK0(c, m) if(c){ASSERT(!(m));}
#else
	#define PTCHK0(c, m) (c);
#endif

namespace WorkerThread {

#define MAX_JOBS 100
Job *jobs[MAX_JOBS];
pthread_mutex_t mtxJobs;

pthread_cond_t cndJobs;
pthread_mutex_t mtxUnclaimed;
int unclaimed_jobs = 0;

//TODO: better allocation strategy for jobs (e.g. ring buffer)
void *job_alloc(size_t size){
	return malloc(size);
}
void job_free(void *ptr){
	free(ptr);
}

void destroy_job(JobHandle handle){
	MICROPROFILE_SCOPEI("WorkerThread", "destroy_job", 0xcb010f);

	Job *j = jobs[handle];
	ASSERT(j != nullptr && "Tried to destroy null job");

	{MICROPROFILE_SCOPEI("WorkerThread", "destroy mutex & cond", 0xcb010f);
		PTCHK0(pthread_mutex_lock(&j->mtx), "cleanupJobs failed lock job's mutex");
		PTCHK0(pthread_mutex_unlock(&j->mtx), "cleanupJobs failed to unlock job's mutex");
		PTCHK0(pthread_mutex_destroy(&j->mtx), "Mutex destruction failed");
		PTCHK0(pthread_cond_destroy(&j->cnd), "Condition variable destruction failed");	
	}

	{MICROPROFILE_SCOPEI("WorkerThread", "free", 0xcb010f);
		job_free((void*)j);
		jobs[handle] = nullptr;
	}
}

Job *popJob(){
	PTCHK0(pthread_mutex_lock(&mtxUnclaimed), "Failed to lock mtxUnclaimed during pop");

	while(unclaimed_jobs <= 0){
		PTCHK0(pthread_cond_wait(&cndJobs, &mtxUnclaimed), "pthread_cond_wait failed for cndJobs");
	}
	unclaimed_jobs--;

	PTCHK0(pthread_mutex_unlock(&mtxUnclaimed), "Failed to unlock mtxUnclaimed during pop");
	PTCHK0(pthread_mutex_lock(&mtxJobs), "Failed to lock mtxJobs during pop");

	//cleanupJobs();

	//find unclaimed job
	Job *ret = nullptr;
	for(int i = 0; i < MAX_JOBS; i++){
		int cur_idx = i;
		if(jobs[cur_idx] == nullptr){
			continue;
		}
		if(!jobs[cur_idx]->claimed && ret == nullptr){
			ret = jobs[cur_idx];
			break;
		}
	}

	ASSERT(ret != nullptr && "Failed to find any unclaimed jobs!");
	ret->claimed = true;

	PTCHK0(pthread_mutex_unlock(&mtxJobs), "Failed to unlock mtxJobs during pop");
	return ret;
}

void setJobFinished(Job *job){
	MICROPROFILE_SCOPEI("WorkerThread", "setJobFinished", 0xff8800);
	PTCHK0(pthread_mutex_lock(&job->mtx), "Failed to lock job's mutex in setJobFinished");
	job->done = 1;
	PTCHK0(pthread_cond_broadcast(&job->cnd), "failed to broadcast job's condition variable");
	PTCHK0(pthread_mutex_unlock(&job->mtx), "failed to unlock job's mutex in setJobFinished");
}

JobHandle _pushJob(Job *job){
	MICROPROFILE_SCOPEI("WorkerThread", "_pushJob", 0x01cb0f);
	//Find free space in jobs array

	{MICROPROFILE_SCOPEI("WorkerThread", "lock mtxJobs", 0xcb010f);
		PTCHK0(pthread_mutex_lock(&mtxJobs), "pushJob failed to lock mtxJobs");
	}

	int idx = -1;
	{MICROPROFILE_SCOPEI("WorkerThread", "find free", 0xcb010f);
		for(int i = 0; i < MAX_JOBS; i++){
			if(jobs[i] == nullptr){
				idx = i;
				break;
			}else if(jobs[i]->done == 2){
				destroy_job(i);
				idx = i;
				break;
			}
		}
	}

	if(idx == -1){
		ASSERT(!"Ran out of space for jobs");
	}else{
		MICROPROFILE_SCOPEI("WorkerThread", "init new job", 0xcb010f);

		job->done = false;
		job->claimed = false;
		job->handle = idx;
		jobs[idx] = job;
		PTCHK0(pthread_mutex_init(&job->mtx, NULL), "Mutex init failed");
		PTCHK0(pthread_cond_init(&job->cnd, NULL), "Condition variable init failed");

		PTCHK0(pthread_mutex_lock(&mtxUnclaimed), "Failed to lock mtxUnclaimed in pushJob");
		unclaimed_jobs++;
		PTCHK0(pthread_cond_broadcast(&cndJobs), "pthread_cond_broadcast failed");
		PTCHK0(pthread_mutex_unlock(&mtxUnclaimed), "Failed to unlock mtxUnclaimed in pushJob");

	}
	PTCHK0(pthread_mutex_unlock(&mtxJobs), "Failed to unlock mtxJobs during push");
	return (JobHandle) idx;
}

thread_local bool alive;
void *run_worker(void *pCtxt){	
    MicroProfileOnThreadCreate("WorkerThread");

	ASSERT(pCtxt == nullptr);

	//LOG("<worker thread spawned>");
	alive = true;
	while(alive){
		MICROPROFILE_SCOPEI("WorkerThread", "loop", 0x01cb0f);
		Job *job = popJob();
		ASSERT(job && "Invalid job");
		job->execute();
		setJobFinished(job);
	}
	return nullptr;
}

std::vector<pthread_t> worker_threads;
void spawnWorkers(int count){
	ASSERT(worker_threads.size() == 0);
	LOG("Spawning %d worker threads", count);
	for(int i = 0; i < count; i++){
		pthread_t tid;
		pthread_create(&tid, NULL, &run_worker, NULL);
		worker_threads.push_back(tid);
	}
}

void killWorkers(){
	for(int i = 0; i < worker_threads.size(); i++){
		submitJob(WRK_DIE);
	}
	for(int i = 0; i < worker_threads.size(); i++){
		pthread_join(worker_threads[i], NULL);
	}
	worker_threads.clear();
}

bool init(){
	if(pthread_cond_init(&cndJobs, NULL) != 0){
		return false;
	}

	if(pthread_mutex_init(&mtxJobs, NULL) != 0){
		return false;
	}

	if(pthread_mutex_init(&mtxUnclaimed, NULL) != 0){
		return false;
	}

	for(int i = 0; i < MAX_JOBS; i++){
		ASSERT(jobs[i] == nullptr);
	}
	return true;
}

void join(JobHandle &handle){
	Job *j = jobs[handle];
	pthread_mutex_lock(&j->mtx);
	while(!j->done){
		pthread_cond_wait(&j->cnd, &j->mtx);
	}
	j->done = 2; //can be destroyed
	pthread_mutex_unlock(&j->mtx);
}

bool tryJoin(JobHandle &handle){
	Job *j = jobs[handle];
	ASSERT(j != nullptr && "job is null!");
	int res = pthread_mutex_trylock(&j->mtx);
	if(res == EBUSY){
		return false;
	}else if(res != 0){
		ASSERT(!"pthread_mutex_trylock failed");
		return false;
	}else{
		bool finished = j->done;
		if(finished){
			j->done = 2;
		}
		pthread_mutex_unlock(&j->mtx);
		return finished;
	}
}
////////////////////////

//kill thread
struct DeathJob: public Job {
	DeathJob():
		Job()
	{
		//--
	}

	~DeathJob()
	{
		//--
	}

	void execute() override {
		alive = false;
	}
};

struct FunctionPointerJob: public Job {
	void (*func)(void);

	FunctionPointerJob(void(*fp)(void)):
		Job(),
		func(fp)
	{
		ASSERT(func != nullptr && "Function pointer for FunctionPointerJob is null");
	}

	~FunctionPointerJob()
	{
		//--
	}

	void execute() override {
		ASSERT(func != nullptr);
		func();
	}
};

struct TestJob: public Job {
	int x, y, z;
	
	TestJob(int X, int Y, int Z):
		Job()
	{
		x = X;
		y = Y;
		z = Z;
	}

	~TestJob()
	{
		//--
	}

	void execute() override {
		MICROPROFILE_SCOPEI("TestJob", "execute", 0x00ffff);
		for(int i = 0; i < 10000000; i++){
			x += i;
		}
	}
};

struct UpdateSkeletonJob: Job {
	int playerId;
	UpdateSkeletonJob(int pid):
		Job(),
		playerId(pid)
	{
		//--
	}

	~UpdateSkeletonJob()
	{
		//--
	}

	void execute() override {
		Person::players[playerId]->UpdateSkeleton();
	}
};

JobHandle submitJob(Job *j){
	return _pushJob(j);
}

JobHandle submitJob(WorkTask type, ...){
    va_list args;
    va_start(args, type);

    JobHandle ret = 0;
    switch(type){
    	default:
    		ASSERT(!"Invalid job type");
    		break;

    	case WRK_UPDATE_SKELETON: {
    		int playerId = va_arg(args, int);
    		ret = submitJob<UpdateSkeletonJob>(playerId);
    		break;
    	}

    	case WRK_DIE:
    		ret = submitJob<DeathJob>();
    		break;

    	case WRK_FUNC:
    		ret = submitJob<FunctionPointerJob>(va_arg(args, void(*)(void)));
    		break;

    	case WRK_TEST: {
    		int x = va_arg(args, int);
    		int y = va_arg(args, int);
    		int z = va_arg(args, int);
    		ret = submitJob<TestJob>(x, y, z);
    		break;
    	}
    }
    va_end(args);

    return ret;
}

}