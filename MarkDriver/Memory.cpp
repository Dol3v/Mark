
#include "Memory.h"
#include "Macros.h"

void* operator new(size_t Size, POOL_FLAGS PoolFlags, ULONG Tag) {
	auto* ptr =  ExAllocatePool2(PoolFlags, Size, Tag);
	if (ptr == nullptr) {
		LOG_FAIL_VA("Allocation of size 0x%llx failed", Size);
	}
	return ptr;
}


void operator delete(PVOID Ptr, size_t) {
	if (Ptr != nullptr) {
		ExFreePool(Ptr);
	}
}