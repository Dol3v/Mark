#pragma once

#include <ntifs.h>

/*
	Implementation of a pushlock
*/
struct PushLock {

	void Lock() {
		::KeEnterCriticalRegion();
		if (isExclusive) {
			::ExAcquirePushLockExclusive(pushLock);
		}
		else {
			::ExAcquirePushLockShared(pushLock);
		}
	}

	void Unlock() {
		if (isExclusive) {
			::ExReleasePushLockExclusive(pushLock);
		}
		else {
			::ExReleasePushLockShared(pushLock);
		}
		::KeLeaveCriticalRegion();
	}

	PEX_PUSH_LOCK pushLock;
	bool isExclusive;
};