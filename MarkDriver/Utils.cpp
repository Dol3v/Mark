
#include "Utils.h"

constexpr ULONG OBJECT_NAME_MAX_SIZE = 1024;

PoolPointer<OBJECT_NAME_INFORMATION> GetObjectName(_In_ const PVOID Object, _Inout_ ULONG& ReturnLength) {
	PoolPointer<OBJECT_NAME_INFORMATION> result(OBJECT_NAME_MAX_SIZE);
	auto status = ::ObQueryNameString(Object, result.get(),
		static_cast<ULONG>(result.length()), &ReturnLength);
	if (!NT_SUCCESS(status)) {
		LOG_FAIL_WITH_STATUS("Object name buffer was too small", status);
		ReturnLength = 0;
	}
	return result;
}
