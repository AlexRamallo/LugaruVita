#ifndef __WORKERTHREAD_H___
#define __WORKERTHREAD_H___
#include <vector>

enum WorkTask {
	WRK_NONE,
	WRK_SKELETON_CALC_NORMALS
};

typedef void* WorkHandle;

class WorkerThread {
public:
	int id;
	bool alive;
	WorkerThread(int id);
	~WorkerThread();

	void operator()();

	static WorkHandle submitJob(WorkTask type, ...);
};

#endif //__WORKERTHREAD_H___