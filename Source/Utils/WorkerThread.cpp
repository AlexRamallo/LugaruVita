#include "Utils/WorkerThread.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>

WorkerThread::WorkerThread(int ID):
	id(ID),
	alive(true)
{
	//--
}

WorkerThread::~WorkerThread()
{
	//--
}

void WorkerThread::operator()(){
	while(alive){
		//...
	}
}