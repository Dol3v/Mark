#pragma once

#include <ntifs.h>

/*
	Mutex wrapper.
*/
struct Mutex {

	void Init() {
		::KeInitializeMutex(&mutex, 0);
	}

	void Lock() {
		::KeWaitForMutexObject(&mutex, Executive, KernelMode, FALSE, nullptr);
	}

	void Unlock() {
		::KeReleaseMutex(&mutex, FALSE);
	}

private:
	KMUTEX mutex;
};
