#pragma once

#include <ntifs.h>

#include "PoolPointer.h"
#include "Macros.h"

/*
	Contains some undocumented functions and a lot of utility functions
*/

extern "C" {
	NTSTATUS ObReferenceObjectByName(_In_ PUNICODE_STRING ObjectName,
		_In_ ULONG Attributes,
		_In_ PACCESS_STATE AccessState,
		_In_opt_ ACCESS_MASK DesiredAccess,
		_In_ POBJECT_TYPE ObjectType,
		_In_ KPROCESSOR_MODE AccessMode,
		_Inout_opt_ PVOID ParseContext,
		_Out_ PVOID* Object);

	POBJECT_TYPE ObGetObjectType(_In_ PVOID Object);
}

namespace Utils {

	/*
		Validates input of fixed size. Assumes METHOD_BUFFERED.

		Returns nullptr on fail.
	*/
	template <typename T>
	T* ValidateFixedSizeInput(PIRP Irp, PIO_STACK_LOCATION CurrentStackLocation) {
		auto& ioctlParameters = CurrentStackLocation->Parameters.DeviceIoControl;
		if (ioctlParameters.InputBufferLength < sizeof(T)) {
			return nullptr;
		}
		return reinterpret_cast<T*>(Irp->AssociatedIrp.SystemBuffer);
	}

	/*
		Validates input of the form

		-------------------------------------------------------------------
		| Fixed Header | ArrayLength | Other Header Stuff | Anysize array |
		-------------------------------------------------------------------

		NOTE: Array length should be ULONG.

		Tip: don't realize you were better using pointers and ProbeForRead 3 months into the project.

		Returns nullptr on fail.
	*/
	template <typename T, typename ArrayType>
	T* ValidateAnysizeInput(PIRP Irp, PIO_STACK_LOCATION CurrentStackLocation, size_t ArrayLengthOffset) {
		auto& ioctlParameters = CurrentStackLocation->Parameters.DeviceIoControl;
		if (ioctlParameters.InputBufferLength < sizeof(T)) {
			return nullptr;
		}
		auto* inputBuffer = Irp->AssociatedIrp.SystemBuffer;
		ULONG arrayLength = *reinterpret_cast<ULONG*>((PUCHAR)inputBuffer + ArrayLengthOffset);
		if (ioctlParameters.InputBufferLength < sizeof(T) - sizeof(ArrayType) + arrayLength * sizeof(ArrayType)) {
			return nullptr;
		}
		return reinterpret_cast<T*>(inputBuffer);
	}

	/*
		Returns the name of an object based off a pointer.

		If the return length is zero an error occurred.
	*/
	PoolPointer<OBJECT_NAME_INFORMATION> GetObjectName(_In_ const PVOID Object, _Inout_ ULONG& ReturnLength);

}
