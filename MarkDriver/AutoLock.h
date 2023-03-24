#pragma once


/*
	RAII wrapper for locks
*/
template <typename Lock>
class AutoLock {
public:
	AutoLock(Lock& lock) : lock(lock) {
		lock.Lock();
	}

	~AutoLock() {
		lock.Unlock();
	}

private:
	Lock& lock;
};
