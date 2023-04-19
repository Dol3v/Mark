#pragma once

#include <ntifs.h>
#include "DriverConfiguration.h"

/*
	Override for operator new.

	Allocates memory in specified pool with given tag.
*/
void* __cdecl operator new(size_t Size, POOL_FLAGS PoolFlags, ULONG Tag = DRIVER_TAG);

/*
	Override for operator delete.

	Frees from pool with specified tag.
*/
void operator delete(PVOID Ptr, size_t);