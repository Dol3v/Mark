#include "DriverLoaderShellcode2.h"
#include "../MarkDriver/DriverConfiguration.h"

#include <exception>

size_t GetLoaderShellcodeSize() {
	return 0x1DE3 - 0x1C80; // hardcoded
}

VOID LoadDriverShellcode(PVOID Address, PVOID KernelImageBase, ULONG32 reflectiveLoaderRva) {
	DECLARE_UNICODE_STRING(uExAllocatePoolWithTag, L"ExAllocatePoolWithTag");
	DECLARE_UNICODE_STRING(uPsCreateSystemThread, L"PsCreateSystemThread");
	auto* MmGetSystemRoutineAddress = (uMmGetSystemRoutineAddress_t)(Address);

	auto* ExAllocatePoolWithTag = (uExAllocatePoolWithTag_t)(MmGetSystemRoutineAddress(&uExAllocatePoolWithTag));
	auto* PsCreateSystemThread = (uPsCreateSystemThread_t)(MmGetSystemRoutineAddress(&uPsCreateSystemThread));
	if (ExAllocatePoolWithTag == nullptr || PsCreateSystemThread == nullptr) return;

	OBJECT_ATTRIBUTES attrs = { 0 };
	InitializeObjectAttributes(&attrs, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);

	auto* context = (LoaderContext*)(ExAllocatePoolWithTag(1, sizeof(LoaderContext), DRIVER_TAG));
	if (context == nullptr) return;

	context->ImageBase = KernelImageBase;
	context->MmGetSystemRoutine = MmGetSystemRoutineAddress;
	HANDLE thread = 0;
	PsCreateSystemThread(&thread, GENERIC_ALL, &attrs, nullptr, nullptr, (PUCHAR)(KernelImageBase)+reflectiveLoaderRva, context);
}