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

#include <SDL2/SDL.h>

#ifndef NDEBUG
	#define PTCHK0(c, m) if(c){ASSERT(!(m));}
#else
	#define PTCHK0(c, m) (c);
#endif

namespace WorkerThread {

#define MAX_JOBS 100

struct JobSync {
	pthread_mutex_t mtx;
	pthread_cond_t cnd;

	void init(){
		PTCHK0(pthread_mutex_init(&mtx, NULL), "Failed to init mutex for JobSync");
		PTCHK0(pthread_cond_init(&cnd, NULL), "Failed to init cond for JobSync");
	}
	void destroy(){
		PTCHK0(pthread_mutex_lock(&mtx), "failed lock JobSync's mutex");
		PTCHK0(pthread_mutex_unlock(&mtx), "failed to unlock JobSync's mutex");
		PTCHK0(pthread_mutex_destroy(&mtx), "JobSync utex destruction failed");
		PTCHK0(pthread_cond_destroy(&cnd), "JobSync condition variable destruction failed");	
	}
};

Job *jobs[MAX_JOBS];
JobSync jobs_sync[MAX_JOBS];
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

JobState Job::getState(bool safe){
	if(safe) PTCHK0(pthread_mutex_lock(&jobs_sync[handle].mtx), "Failed to lock job's mutex");
	JobState ret = state;
	if(safe) PTCHK0(pthread_mutex_unlock(&jobs_sync[handle].mtx), "Failed to unlock job's mutex");
	return ret;
}

void Job::setState(JobState set, bool safe){
	if(safe) PTCHK0(pthread_mutex_lock(&jobs_sync[handle].mtx), "Failed to lock job's mutex");
	state = set;
	if(safe) PTCHK0(pthread_mutex_unlock(&jobs_sync[handle].mtx), "Failed to unlock job's mutex");
}

void destroy_job(JobHandle handle){
	MICROPROFILE_SCOPEI("WorkerThread", "destroy_job", 0xcb010f);

	Job *j = jobs[handle];
	ASSERT(j != nullptr && "Tried to destroy null job");

	PTCHK0(pthread_mutex_lock(&jobs_sync[handle].mtx), "destroy_job failed lock job's mutex");
	
	job_free((void*)j);
	jobs[handle] = nullptr;

	PTCHK0(pthread_mutex_unlock(&jobs_sync[handle].mtx), "destroy_job failed to unlock job's mutex");
}

Job *popJob(bool blocking = true){
	PTCHK0(pthread_mutex_lock(&mtxUnclaimed), "Failed to lock mtxUnclaimed during pop");

	while(unclaimed_jobs <= 0){
		if(!blocking){
			PTCHK0(pthread_mutex_unlock(&mtxUnclaimed), "Failed to unlock mtxUnclaimed during pop");
			return nullptr;
		}
		PTCHK0(pthread_cond_wait(&cndJobs, &mtxUnclaimed), "pthread_cond_wait failed for cndJobs");
	}
	unclaimed_jobs--;

	ASSERT(unclaimed_jobs >= 0 && "unclaimed_jobs is negative");

	PTCHK0(pthread_mutex_unlock(&mtxUnclaimed), "Failed to unlock mtxUnclaimed during pop");
	PTCHK0(pthread_mutex_lock(&mtxJobs), "Failed to lock mtxJobs during pop");

	//find unclaimed job
	Job *ret = nullptr;
	for(int i = 0; i < MAX_JOBS; i++){
		int cur_idx = i;
		if(jobs[cur_idx] == nullptr){
			continue;
		}
		//if(jobs[cur_idx]->state == JS_UNCLAIMED && ret == nullptr){
		if(jobs[cur_idx]->getState() == JS_UNCLAIMED && ret == nullptr){
			ret = jobs[cur_idx];
			break;
		}
	}

	ASSERT(ret != nullptr && "Failed to find any unclaimed jobs!");
	//ret->state = JS_CLAIMED;
	ret->setState(JS_CLAIMED);

	PTCHK0(pthread_mutex_unlock(&mtxJobs), "Failed to unlock mtxJobs during pop");
	return ret;
}

void setJobFinished(Job *job){
	MICROPROFILE_SCOPEI("WorkerThread", "setJobFinished", 0xff8800);
	PTCHK0(pthread_mutex_lock(&jobs_sync[job->handle].mtx), "Failed to lock job's mutex in setJobFinished");
	job->state = JS_FINISHED;

	//unblock any dependents
	int unblock_count = 0;
	for(int i = 0; i < MAX_DEPENDENTS; i++){
		Job *dep = job->dependents[i];
		if(dep != nullptr){
			//dep->state = JS_UNCLAIMED;
			dep->setState(JS_UNCLAIMED);
			unblock_count++;
		}
	}

	//signal that there are new unclaimed jobs
	if(unblock_count > 0){
		PTCHK0(pthread_mutex_lock(&mtxUnclaimed), "Failed to lock mtxUnclaimed during dep unblock");
		unclaimed_jobs += unblock_count;
		PTCHK0(pthread_cond_broadcast(&cndJobs), "Fail to broadcast mtxUnclaimed");
		PTCHK0(pthread_mutex_unlock(&mtxUnclaimed), "Failed to unlock mtxUnclaimed during dep unblock");
	}

	//signal that this job has changed state
	PTCHK0(pthread_cond_signal(&jobs_sync[job->handle].cnd), "failed to broadcast job's condition variable");
	PTCHK0(pthread_mutex_unlock(&jobs_sync[job->handle].mtx), "failed to unlock job's mutex in setJobFinished");
}

JobHandle _pushJob(Job *job, JobHandle parent){
	MICROPROFILE_SCOPEI("WorkerThread", "_pushJob", 0x01cb0f);
	
	//Find free space in jobs array
	PTCHK0(pthread_mutex_lock(&mtxJobs), "pushJob failed to lock mtxJobs");
	int idx = -1;

	for(int i = 0; i < MAX_JOBS; i++){
		if(jobs[i] == nullptr){
			idx = i;
			break;
		//}else if(jobs[i]->state == JS_JOINED){
		}else if(jobs[i]->getState() == JS_JOINED){
			destroy_job(i);
			idx = i;
			break;
		}
	}

	if(idx == -1){
		ASSERT(!"Ran out of space for jobs");
	}else{
		MICROPROFILE_SCOPEI("WorkerThread", "init new job", 0xcb010f);

		bool is_blocked = false;
		if(parent >= 0){
			Job *pj = jobs[parent];
			if(pj == nullptr){
				//parent was already destroyed, meaning it completed at some point
				//job->state = JS_UNCLAIMED;
				job->setState(JS_UNCLAIMED);
			}else{
				//parent exists, need to check if it completed
				JobState pst = pj->getState();
				switch(pst){
					case JS_DEAD:
					case JS_FINISHED:
					case JS_JOINED:
						//parent completed, no need to block
						job->setState(JS_UNCLAIMED);
						break;
					
					case JS_UNCLAIMED:
					case JS_CLAIMED:
					case JS_BLOCKED:
						is_blocked = true;
						//parent hasn't completed, block and add us as a dependent
						job->setState(JS_BLOCKED);
						PTCHK0(pthread_mutex_lock(&jobs_sync[parent].mtx), "Failed to lock parent task's mutex");
						bool added = false;
						for(int i = 0; i < MAX_DEPENDENTS; i++){
							if(pj->dependents[i] == nullptr){
								pj->dependents[i] = job;
								added = true;
								break;
							}
						}
						PTCHK0(pthread_mutex_unlock(&jobs_sync[parent].mtx), "Failed to unlock parent task's mutex");
						ASSERT(added && "Parent job has no more free dependent slots");
						break;
				}
			}
		}else{
			job->setState(JS_UNCLAIMED);
		}
		job->handle = idx;
		for(int i = 0; i < MAX_DEPENDENTS; i++){
			job->dependents[i] = nullptr;
		}
		jobs[idx] = job;

		if(!is_blocked){
			PTCHK0(pthread_mutex_lock(&mtxUnclaimed), "Failed to lock mtxUnclaimed in pushJob");
			unclaimed_jobs++;
			PTCHK0(pthread_cond_signal(&cndJobs), "pthread_cond_signal failed");
			PTCHK0(pthread_mutex_unlock(&mtxUnclaimed), "Failed to unlock mtxUnclaimed in pushJob");
		}
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
		jobs_sync[i].init();
	}
	return true;
}

void join(JobHandle &handle, bool work){
	if(work){
		MICROPROFILE_SCOPEI("WorkerThread", "join-work", 0x66ffaa);
		//only spin for a limited time
		for(int i = 0; i < 5; i++){
			MICROPROFILE_SCOPEI("WorkerThread", "spin", 0x66ffaa);
			if(tryJoin(handle)){
				return;
			}else{
				//do some work
				Job *job = popJob(false);
				if(job != nullptr){
					job->execute();
					setJobFinished(job);
				}
			}
		}
	}
	//regular blocking join
	Job *j = jobs[handle];
	pthread_mutex_lock(&jobs_sync[handle].mtx);
	while(j->state != JS_FINISHED){
		pthread_cond_wait(&jobs_sync[handle].cnd, &jobs_sync[handle].mtx);
	}
	j->state = JS_JOINED; //can be destroyed
	pthread_mutex_unlock(&jobs_sync[handle].mtx);
}

bool tryJoin(JobHandle &handle){
	Job *j = jobs[handle];
	ASSERT(j != nullptr && "job is null!");
	int res = pthread_mutex_trylock(&jobs_sync[handle].mtx);
	if(res == EBUSY){
		return false;
	}else if(res != 0){
		ASSERT(!"pthread_mutex_trylock failed");
		return false;
	}else{
		bool finished = (j->state == JS_FINISHED);
		if(finished){
			j->state = JS_JOINED;
		}
		pthread_mutex_unlock(&jobs_sync[handle].mtx);
		return finished;
	}
}
////////////////////////

struct FunctionPointerJob: public Job {
	void (*func)(void);

	FunctionPointerJob(void(*fp)(void)):
		Job(),
		func(fp)
	{
		ASSERT(func != nullptr && "Function pointer for FunctionPointerJob is null");
	}

	~FunctionPointerJob() {/***/}

	void execute() override {
		ASSERT(func != nullptr);
		func();
	}
};

//kill thread
struct DeathJob: public Job {
	DeathJob():Job(){}
	~DeathJob(){}
	void execute() override {
		alive = false;
	}
};

struct UpdateSkeletonJob: Job {
	UpdateSkeletonJob(int pid):Job(), playerId(pid) {/***/}
	~UpdateSkeletonJob() {/***/}
	int playerId;
	void execute() override {
		Person::players[playerId]->UpdateSkeleton();
	}
};

struct UpdateSkeletonNormalsJob: Job {
	UpdateSkeletonNormalsJob(int pid):Job(), playerId(pid) {/***/}
	~UpdateSkeletonNormalsJob() {/***/}
	int playerId;
	void execute() override {
		Person::players[playerId]->UpdateNormals();
	}
};

JobHandle submitJob(Job *j){
	return _pushJob(j, -1);
}

#define DO_SUBMIT_JOB(T, ...) \
	if(parent >= 0) {\
		ret = submitDependentJob<T>(parent, ## __VA_ARGS__);\
	}else{\
		ret = submitJob<T>(__VA_ARGS__);\
	}

JobHandle vsubmitJob(JobHandle parent, WorkTask type, va_list args){
    JobHandle ret = 0;
    switch(type){
    	default:
    		ASSERT(!"Invalid job type");
    		break;

    	case WRK_DIE:
    		//ret = submitJob<DeathJob>();
    		DO_SUBMIT_JOB(DeathJob);
    		break;

    	case WRK_UPDATE_SKELETON: {
    		int playerId = va_arg(args, int);
    		//ret = submitJob<UpdateSkeletonJob>(playerId);
    		DO_SUBMIT_JOB(UpdateSkeletonJob, playerId);
    		break;
    	}

    	case WRK_UPDATE_SKELETON_NORMALS: {
    		int playerId = va_arg(args, int);
    		//ret = submitJob<UpdateSkeletonNormalsJob>(playerId);
    		DO_SUBMIT_JOB(UpdateSkeletonNormalsJob, playerId);
    		break;
    	}

    	case WRK_FUNC:
    		//ret = submitJob<FunctionPointerJob>(va_arg(args, void(*)(void)));
    		DO_SUBMIT_JOB(FunctionPointerJob, va_arg(args, void(*)(void)));
    		break;
    }
    va_end(args);
    return ret;
}

JobHandle submitDependentJob(JobHandle parent, WorkTask type, ...){
    va_list args;
    va_start(args, type);
    JobHandle ret = vsubmitJob(parent, type, args);
    va_end(args);
    return ret;
}

JobHandle submitJob(WorkTask type, ...){
    va_list args;
    va_start(args, type);
    JobHandle ret = vsubmitJob(-1, type, args);
    va_end(args);
    return ret;
}


} //namespace WorkerThread