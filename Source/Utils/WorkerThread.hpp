#ifndef __WORKERTHREAD_H___
#define __WORKERTHREAD_H___
#include "Utils/Log.h"

namespace WorkerThread{
	static const int MAX_DEPENDENTS = 16;

	enum WorkTask {
		WRK_NONE,
		WRK_USER,
		WRK_FUNC,
		WRK_DIE,
		WRK_UPDATE_SKELETON,
		WRK_UPDATE_SKELETON_NORMALS,
		WRK_LOAD_IMAGE,
		WRK_CALC_OCCLUSION
	};

	enum JobState {
		//job object ready to be reused
		JS_DEAD = 0,

		//job cannot be claimed because a dependency hasn't finished
		JS_BLOCKED,

		//job is ready to be claimed by a worker
		JS_UNCLAIMED,

		//job has been claimed by a worker (and is probably executing)
		JS_CLAIMED,

		//job has finished executing, but hasn't been joined
		JS_FINISHED,

		//job has been joined and is ready to be destroyed
		JS_JOINED
	};

	typedef int JobHandle;

	struct Job {
		WorkTask type;
		JobHandle handle;
		
		/**
		 * Dependent jobs cannot be claimed by workers
		 * until the parent job is finished
		 * */
		Job *dependents[MAX_DEPENDENTS];
		Job *parent; //only valid while state is JS_BLOCKED

		JobState state;
		void setState(JobState set, bool safe = true);
		JobState getState(bool safe = true);

		Job() = default;
		virtual ~Job() = default;

		virtual void execute() = 0;
	};

	//internal use only!
	JobHandle _pushJob(Job *job, JobHandle parent);

	JobHandle submitJob(WorkTask type, ...);
	JobHandle submitDependentJob(JobHandle parent, WorkTask type, ...);

	void *job_alloc(size_t size);
	void job_free(void *ptr);

	template<typename T, typename... Args>
	JobHandle submitJob(Args... args){
		void *jm = job_alloc(sizeof(T));
		ASSERT(jm != nullptr && "Failed to allocate job instance");
		T *job = new(jm) T(args...);
		job->type = WRK_USER;
		JobHandle ret = _pushJob(job, -1);
		if(ret == -1){
			job_free(jm);
			ASSERT(!"Job queue is full");
		}
		return ret;
	}

	/**
	 * Submit a job that won't run until its parent completes
	 * 
	 * The parent job MUST NOT have been joined before calling this
	 * */
	template<typename T, typename... Args>
	JobHandle submitDependentJob(JobHandle parent, Args... args){
		void *jm = job_alloc(sizeof(T));
		ASSERT(jm != nullptr && "Failed to allocate job instance");
		T *job = new(jm) T(args...);
		job->type = WRK_USER;
		JobHandle ret = _pushJob(job, parent);
		if(ret == -1){
			job_free(jm);
			ASSERT(!"Job queue is full");
		}
		return ret;
	}


	/**
	 * MUST call with a valid handle.
	 * 
	 * Sleeps calling thread until job joins
	 * 
	 * handle is invalidated upon return
	 * 
	 * if `work` is true, this thread will act as a
	 * worker thread as it waits
	 * */
	void join(JobHandle &handle, bool work = true);

	/**
	 * MUST call with a valid handle.
	 *
	 * If join succeeds, handle becomes invalid
	 * */
	bool tryJoin(JobHandle &handle);

	void spawnWorkers(int count);
	void killWorkers();

	bool init();
}

#endif //__WORKERTHREAD_H___