#ifndef __WORKERTHREAD_H___
#define __WORKERTHREAD_H___
#include "Utils/Log.h"

namespace WorkerThread{
	enum WorkTask {
		WRK_NONE,
		WRK_USER,
		WRK_FUNC,
		WRK_DIE,
		WRK_TEST,
		WRK_UPDATE_SKELETON
	};

	typedef int JobHandle;

	struct Job {
		WorkTask type;
		JobHandle handle;
		//pthread_mutex_t mtx;
		//pthread_cond_t cnd;
		bool claimed;
		char done;

		Job() = default;
		virtual ~Job() = default;

		virtual void execute() = 0;
	};

	//internal use only!
	JobHandle _pushJob(Job *job);

	JobHandle submitJob(WorkTask type, ...);

	void *job_alloc(size_t size);
	void job_free(void *ptr);

	template<typename T, typename... Args>
	JobHandle submitJob(Args... args){
		void *jm = job_alloc(sizeof(T));
		ASSERT(jm != nullptr && "Failed to allocate job instance");
		T *job = new(jm) T(args...);
		job->type = WRK_USER;
		JobHandle ret = _pushJob(job);
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
	 * */
	void join(JobHandle &handle);

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