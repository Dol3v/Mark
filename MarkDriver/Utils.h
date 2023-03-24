#pragma once

#include <ntifs.h>

#include "PoolPointer.h"
#include "Macros.h"

/*
	Contains some undocumented functions and stuff
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
		Returns the name of an object based off a pointer.

		If the return length is zero an error occurred.
	*/
	PoolPointer<OBJECT_NAME_INFORMATION> GetObjectName(_In_ const PVOID Object, _Inout_ ULONG& ReturnLength);
}
