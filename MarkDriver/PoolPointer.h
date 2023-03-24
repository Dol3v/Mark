#pragma once

#include <ntifs.h>

#include "Macros.h"

template <typename T, POOL_TYPE PoolType = PagedPool, ULONG PoolTag = DRIVER_TAG>
class PoolPointer {
private:
	T* ptr;
	size_t size;

	void Free() {
		if (this->ptr != nullptr) {
			::ExFreePoolWithTag(this->ptr, PoolTag);
		}
	}

public:
	explicit PoolPointer(size_t BufferSize = sizeof(T)) : ptr(nullptr), size(0) {
		this->ptr = reinterpret_cast<T*>(::ExAllocatePoolWithTag(PoolType, BufferSize, PoolTag));
		if (this->ptr == nullptr) {
			LOG_FAIL("Couldn't allocate buffer");
			::ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
		}
		this->size = BufferSize;
	}

	PoolPointer(const PoolPointer& Other) {
		this->ptr = Other.ptr;
	}

	PoolPointer& operator=(const PoolPointer& Other) {
		this->Free();
		this->ptr = Other.ptr;
		this->size = Other.size;
		return *this;
	}

	PoolPointer(PoolPointer&& Other) {
		this->ptr = Other.ptr;
		this->size = Other.size;

		Other.ptr = nullptr;
		Other.size = 0;
	}

	PoolPointer& operator=(const PoolPointer&& Other) {
		this->Free();
		this->ptr = Other.ptr;
		this->size = Other.size;

		Other.size = 0;
		Other.ptr = nullptr;
		return *this;
	}

	T* get() {
		return this->ptr;
	}

	size_t length() const {
		return this->size;
	}

	T* operator->() const {
		return this->ptr;
	}

	T& operator*() const {
		return *this->ptr;
	}

	~PoolPointer() {
		this->Free();
	}
};
