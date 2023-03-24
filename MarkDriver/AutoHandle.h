#pragma once

#include <ntifs.h>

#include "Macros.h"

/*
	RAII wrapper for object handles.
*/
class AutoHandle {
private:
	HANDLE handle;

public:
	AutoHandle::AutoHandle(HANDLE handle) : handle(handle) {}

	/*
		Creates a kernel handle from a pointer.

		Can raise exception in case of failure.
	*/
	AutoHandle::AutoHandle(PVOID ObjectPointer, POBJECT_TYPE Type, ACCESS_MASK Access = GENERIC_ALL) {
		auto status = ::ObOpenObjectByPointer(
			ObjectPointer,
			OBJ_KERNEL_HANDLE,
			nullptr,
			Access,
			Type,
			KernelMode,
			&this->handle
		);
		if (!NT_SUCCESS(status)) {
			LOG_FAIL_VA("Failed to open object %p, status=0x%08x", ObjectPointer, status);
			::ExRaiseStatus(status);
		}
	}

	// disable copying
	AutoHandle::AutoHandle(const AutoHandle&) = delete;
	AutoHandle& operator=(const AutoHandle&) = delete;

	HANDLE get() {
		return this->handle;
	}

	AutoHandle::~AutoHandle() {
		::ZwClose(this->handle);
	}
};
